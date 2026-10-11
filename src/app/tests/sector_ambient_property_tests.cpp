#include "test_common.hpp"

#include "main_window/main_window.hpp"
#include "gl_view.hpp"
#include "sed/level.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include <QApplication>
#include <QColor>
#include <QLineEdit>
#include <QMetaObject>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QTreeWidget>
#include <QToolButton>

#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace sed::app_tests
{

ui::ScrubFloatEdit* findIntensityEditorForTest(
    MainWindow& window, const QString& label)
{
    auto* properties = window.findChild<QTreeWidget*>(
        QStringLiteral("propertiesTree"));
    if (!properties)
    {
        return nullptr;
    }

    std::function<ui::ScrubFloatEdit*(QTreeWidgetItem*)> findInItem =
        [&](QTreeWidgetItem* item) -> ui::ScrubFloatEdit*
    {
        if (!item)
        {
            return nullptr;
        }

        if (item->text(0).trimmed() == label)
        {
            QWidget* editor = properties->itemWidget(item, 1);
            if (editor)
            {
                if (auto* scrub = editor->findChild<ui::ScrubFloatEdit*>())
                {
                    return scrub;
                }
            }
        }

        for (int childIndex = 0; childIndex < item->childCount(); ++childIndex)
        {
            if (auto* scrub = findInItem(item->child(childIndex)))
            {
                return scrub;
            }
        }
        return nullptr;
    };

    for (int row = 0; row < properties->topLevelItemCount(); ++row)
    {
        if (auto* scrub = findInItem(properties->topLevelItem(row)))
        {
            return scrub;
        }
    }
    return nullptr;
}

ui::ScrubFloatEdit* findAmbientIntensityEditorForTest(MainWindow& window)
{
    return findIntensityEditorForTest(window, QStringLiteral("Ambient"));
}

ui::ScrubFloatEdit* findExtraIntensityEditorForTest(MainWindow& window)
{
    return findIntensityEditorForTest(window, QStringLiteral("Extra"));
}

QPushButton* findColorButtonForTest(
    MainWindow& window, const QString& label)
{
    auto* properties = window.findChild<QTreeWidget*>(
        QStringLiteral("propertiesTree"));
    if (!properties)
    {
        return nullptr;
    }

    std::function<QPushButton*(QTreeWidgetItem*)> findInItem =
        [&](QTreeWidgetItem* item) -> QPushButton*
    {
        if (!item)
        {
            return nullptr;
        }

        if (item->text(0).trimmed() == label)
        {
            if (QWidget* editor = properties->itemWidget(item, 1))
            {
                if (auto* button = editor->findChild<QPushButton*>())
                {
                    if (button->property("sedColorSwatch").toBool())
                    {
                        return button;
                    }
                }
            }
        }

        for (int childIndex = 0; childIndex < item->childCount(); ++childIndex)
        {
            if (auto* button = findInItem(item->child(childIndex)))
            {
                return button;
            }
        }
        return nullptr;
    };

    for (int row = 0; row < properties->topLevelItemCount(); ++row)
    {
        if (auto* button = findInItem(properties->topLevelItem(row)))
        {
            return button;
        }
    }
    return nullptr;
}

void runSectorAmbientPropertyTests()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);

    editor::DocumentState state{};
    state.level.kind = ProjectType::IJIM;
    state.level.layers.emplace_back("Layer0");

    Sector sector{};
    sector.ambient = ColorF{ 0.8f, 0.7f, 0.6f, 0.0f };
    sector.vertices = {
        Vertex{ 0.0, 0.0, 0.0, 0 },
        Vertex{ 1.0, 0.0, 0.0, 0 },
    };

    Surface surface{};
    surface.vertices = { 0, 1 };
    surface.texVertices = {
        TexVertex{ 0.0f, 0.0f, ColorF{ 0.6f, 0.2f, 0.1f, 0.25f } },
        TexVertex{ 1.0f, 0.0f, ColorF{ 0.2f, 0.4f, 0.3f, 0.75f } },
    };
    surface.extraLight = ColorF{ 0.05f, 0.05f, 0.05f, 1.0f };
    sector.surfaces.push_back(std::move(surface));

    state.level.sectors.push_back(std::move(sector));
    state.selection.sectors.insert(0);
    window.setDocumentStateForTest(std::move(state));

    auto* button = window.findChild<QToolButton*>(
        QStringLiteral("sectorAmbientFromSurfacesButton"));
    check(button != nullptr
            && !button->icon().isNull()
            && button->toolTip().contains(QStringLiteral("surface colors"), Qt::CaseInsensitive),
        "sector properties expose an icon action for deriving ambient from surface colors");

    if (!button)
    {
        return;
    }

    GlView* viewport = window.findChild<GlView*>();
    if (viewport)
    {
        viewport->notifyLevelGeometryChanged();
        check(!viewport->lastMeshRebuildWasFullForTest(),
            "sector ambient viewport regression test establishes a partial-rebuild baseline");
    }

    button->click();

    const Sector& calculated = window.documentStateForTest().level.sectors[0];
    check(close(calculated.ambient.r, 0.4f)
            && close(calculated.ambient.g, 0.3f)
            && close(calculated.ambient.b, 0.2f)
            && close(calculated.ambient.a, 0.0f),
        "sector ambient action applies original SED baked-vertex averaging semantics");
    check(window.undoDepthForTest() == 1,
        "sector ambient action creates one undo transaction");
    check(viewport != nullptr && viewport->lastMeshRebuildWasFullForTest(),
        "sector ambient action immediately refreshes both level and Thing viewport lighting");
    check(window.undoForTest()
            && close(window.documentStateForTest().level.sectors[0].ambient.r, 0.8f)
            && close(window.documentStateForTest().level.sectors[0].ambient.g, 0.7f)
            && close(window.documentStateForTest().level.sectors[0].ambient.b, 0.6f),
        "sector ambient action is undoable");

    MainWindow ijimAmbientWindow{};
    ijimAmbientWindow.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState ijimAmbientState{};
    ijimAmbientState.level.kind = ProjectType::IJIM;
    ijimAmbientState.level.layers.emplace_back("Layer0");
    ijimAmbientState.level.sectors.resize(2);
    for (Sector& ijimSector : ijimAmbientState.level.sectors)
    {
        ijimSector.ambient = ColorF{ 0.1f, 0.2f, 0.3f, 0.0f };
    }
    ijimAmbientState.selection.sectors.insert(0);
    ijimAmbientState.selection.sectors.insert(1);
    ijimAmbientWindow.setDocumentStateForTest(std::move(ijimAmbientState));

    const ColorF ijimAmbientBefore = ijimAmbientWindow.documentStateForTest()
        .level.sectors[0].ambient;
    QPushButton* ijimAmbientButton = findColorButtonForTest(
        ijimAmbientWindow, QStringLiteral("Ambient"));
    check(ijimAmbientButton != nullptr,
        "IJIM sector Ambient exposes a nested live color swatch");
    if (ijimAmbientButton)
    {
        ijimAmbientButton->click();
    }
    auto* ijimAmbientPopup = dynamic_cast<ui::LiveColorPopup*>(
        ijimAmbientWindow.findChild<QWidget*>(QStringLiteral("liveColorPopup")));
    check(ijimAmbientPopup != nullptr,
        "IJIM sector Ambient swatch opens the live color popup");
    if (ijimAmbientPopup)
    {
        ijimAmbientPopup->setCurrentColor(
            QColor::fromRgbF(0.7, 0.2, 0.9), true);
        QApplication::processEvents();
    }
    const auto ijimColorMatches = [](const ColorF& actual,
                                     const ColorF& expected) {
        return std::abs(actual.r - expected.r) <= 1.0e-5f
            && std::abs(actual.g - expected.g) <= 1.0e-5f
            && std::abs(actual.b - expected.b) <= 1.0e-5f
            && std::abs(actual.a - expected.a) <= 1.0e-5f;
    };
    const auto ijimAmbientMatches = [&](const ColorF& expected) {
        const auto& sectors = ijimAmbientWindow.documentStateForTest()
            .level.sectors;
        return sectors.size() == 2
            && ijimColorMatches(sectors[0].ambient, expected)
            && ijimColorMatches(sectors[1].ambient, expected);
    };
    const ColorF ijimAmbientChanged{
        0.7f, 0.2f, 0.9f, ijimAmbientBefore.a };
    check(ijimAmbientMatches(ijimAmbientChanged)
            && ijimAmbientWindow.undoDepthForTest() == 1,
        "IJIM sector Ambient color applies to every selected sector in one undo step");
    if (ijimAmbientPopup)
    {
        ijimAmbientPopup->close();
        QApplication::processEvents();
    }
    check(ijimAmbientWindow.undoForTest()
            && ijimAmbientMatches(ijimAmbientBefore),
        "IJIM sector Ambient color restores every RGB and alpha channel on undo");

    MainWindow hdrWindow{};
    hdrWindow.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState hdrState{};
    hdrState.level.kind = ProjectType::JKDF2;
    hdrState.level.layers.emplace_back("Layer0");
    hdrState.level.sectors.resize(2);
    for (Sector& hdrSector : hdrState.level.sectors)
    {
        hdrSector.ambient = ColorF{ 1.38f, 1.38f, 1.38f, 0.0f };
    }
    hdrState.selection.sectors.insert(0);
    hdrState.selection.sectors.insert(1);
    hdrWindow.setDocumentStateForTest(std::move(hdrState));

    ui::ScrubFloatEdit* ambientEditor =
        findAmbientIntensityEditorForTest(hdrWindow);
    check(ambientEditor != nullptr
            && ambientEditor->value() == static_cast<double>(1.38f),
        "sector ambient HDR editor displays a value above one without clamping");

    if (ambientEditor)
    {
        ambientEditor->lineEdit()->setText(QStringLiteral("23.44"));
        QMetaObject::invokeMethod(
            ambientEditor->lineEdit(), "textEdited", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("23.44")));
        QMetaObject::invokeMethod(
            ambientEditor->lineEdit(), "editingFinished", Qt::DirectConnection);
    }
    const editor::DocumentState& committed = hdrWindow.documentStateForTest();
    check(committed.level.sectors.size() == 2
            && committed.level.sectors[0].ambient.r == 23.44f
            && committed.level.sectors[1].ambient.r == 23.44f
            && committed.level.sectors[0].ambient.g == 23.44f
            && committed.level.sectors[0].ambient.b == 23.44f
            && committed.level.sectors[0].ambient.a == 0.0f
            && committed.level.sectors[1].ambient.g == 23.44f
            && committed.level.sectors[1].ambient.b == 23.44f
            && committed.level.sectors[1].ambient.a == 0.0f
            && hdrWindow.undoDepthForTest() == 1,
        "sector ambient HDR edit commits the exact value to every selected sector in one undo step");
    check(hdrWindow.undoForTest()
            && hdrWindow.documentStateForTest().level.sectors[0].ambient.r == 1.38f
            && hdrWindow.documentStateForTest().level.sectors[0].ambient.g == 1.38f
            && hdrWindow.documentStateForTest().level.sectors[0].ambient.b == 1.38f
            && hdrWindow.documentStateForTest().level.sectors[0].ambient.a == 0.0f
            && hdrWindow.documentStateForTest().level.sectors[1].ambient.r == 1.38f
            && hdrWindow.documentStateForTest().level.sectors[1].ambient.g == 1.38f
            && hdrWindow.documentStateForTest().level.sectors[1].ambient.b == 1.38f
            && hdrWindow.documentStateForTest().level.sectors[1].ambient.a == 0.0f
            && hdrWindow.redoForTest()
            && hdrWindow.documentStateForTest().level.sectors[0].ambient.r == 23.44f
            && hdrWindow.documentStateForTest().level.sectors[0].ambient.g == 23.44f
            && hdrWindow.documentStateForTest().level.sectors[0].ambient.b == 23.44f
            && hdrWindow.documentStateForTest().level.sectors[0].ambient.a == 0.0f
            && hdrWindow.documentStateForTest().level.sectors[1].ambient.r == 23.44f
            && hdrWindow.documentStateForTest().level.sectors[1].ambient.g == 23.44f
            && hdrWindow.documentStateForTest().level.sectors[1].ambient.b == 23.44f
            && hdrWindow.documentStateForTest().level.sectors[1].ambient.a == 0.0f,
        "sector ambient HDR edit preserves exact values through undo and redo");

    editor::DocumentState negativeState{};
    negativeState.level.kind = ProjectType::JKDF2;
    negativeState.level.layers.emplace_back("Layer0");
    negativeState.level.sectors.emplace_back();
    negativeState.level.sectors[0].ambient = ColorF{ -0.75f, -0.75f, -0.75f, 0.0f };
    negativeState.selection.sectors.insert(0);
    hdrWindow.setDocumentStateForTest(std::move(negativeState));
    ambientEditor = findAmbientIntensityEditorForTest(hdrWindow);
    check(ambientEditor != nullptr
            && ambientEditor->value() == static_cast<double>(-0.75f),
        "sector ambient HDR editor retains a negative authored value");

    const ColorF ambientBeforeInvalid = hdrWindow.documentStateForTest()
        .level.sectors[0].ambient;
    const std::size_t ambientHistoryBeforeInvalid =
        hdrWindow.undoDepthForTest();
    const double maxFloat =
        static_cast<double>(std::numeric_limits<float>::max());
    const double invalidValues[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        maxFloat * 2.0,
        -maxFloat * 2.0,
    };
    bool ambientInvalidValuesRejected = ambientEditor != nullptr;
    for (const double invalidValue : invalidValues)
    {
        ambientEditor = findAmbientIntensityEditorForTest(hdrWindow);
        if (!ambientEditor)
        {
            ambientInvalidValuesRejected = false;
            break;
        }
        ambientEditor->valueCommitted(invalidValue);
        QApplication::processEvents();
        const ColorF current = hdrWindow.documentStateForTest()
            .level.sectors[0].ambient;
        ambientInvalidValuesRejected = ambientInvalidValuesRejected
            && current == ambientBeforeInvalid
            && hdrWindow.undoDepthForTest() == ambientHistoryBeforeInvalid;
    }
    check(ambientInvalidValuesRejected,
        "sector ambient rejects nonfinite and out-of-float-range commits without changing RGB, alpha, or history");

    constexpr float wideScalar = 3.0e38f;
    MainWindow wideWindow{};
    wideWindow.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState wideState{};
    wideState.level.kind = ProjectType::JKDF2;
    wideState.level.layers.emplace_back("Layer0");
    wideState.level.sectors.resize(2);
    for (Sector& wideSector : wideState.level.sectors)
    {
        wideSector.ambient = ColorF{ wideScalar, wideScalar, wideScalar, 0.0f };
        wideSector.extraLight =
            ColorF{ wideScalar, wideScalar, wideScalar, 0.0f };
    }
    wideState.selection.sectors.insert(0);
    wideState.selection.sectors.insert(1);
    wideWindow.setDocumentStateForTest(std::move(wideState));

    const auto isWideFinite = [wideScalar](ui::ScrubFloatEdit* editor) {
        if (!editor)
        {
            return false;
        }
        const double value = editor->value();
        const double expected = static_cast<double>(wideScalar);
        return std::isfinite(value) && value == expected;
    };
    check(isWideFinite(findAmbientIntensityEditorForTest(wideWindow)),
        "sector ambient averages near-float-maximum RGB in double precision without displaying infinity");
    check(isWideFinite(findExtraIntensityEditorForTest(wideWindow)),
        "sector extra-light averages near-float-maximum RGB in double precision without displaying infinity");

    const auto runClassicSameMeanChannelChange =
        [](const bool ambient, const char* message) {
            MainWindow window{};
            window.setAttribute(Qt::WA_DontShowOnScreen);
            editor::DocumentState state{};
            state.level.kind = ProjectType::JKDF2;
            state.level.layers.emplace_back("Layer0");
            Sector& sector = state.level.sectors.emplace_back();
            sector.ambient = ColorF{ 1.0f, 2.0f, 3.0f, 0.0f };
            sector.extraLight = ColorF{ 1.0f, 2.0f, 3.0f, 0.0f };
            state.selection.sectors.insert(0);
            state.selection.sectors.insert(1);
            window.setDocumentStateForTest(std::move(state));

            ui::ScrubFloatEdit* editor = ambient
                ? findAmbientIntensityEditorForTest(window)
                : findExtraIntensityEditorForTest(window);
            check(editor != nullptr && close(editor->value(), 2.0),
                ambient
                    ? "sector Ambient shows the average of imported unequal RGB channels"
                    : "sector Extra shows the average of imported unequal RGB channels");
            if (editor)
            {
                editor->valueCommitted(2.0);
                QApplication::processEvents();
            }

            const ColorF expected{ 2.0f, 2.0f, 2.0f, 0.0f };
            const ColorF& changed = ambient
                ? window.documentStateForTest().level.sectors[0].ambient
                : window.documentStateForTest().level.sectors[0].extraLight;
            check(changed == expected && window.undoDepthForTest() == 1,
                message);
            const bool undone = window.undoForTest();
            const ColorF& restored = ambient
                ? window.documentStateForTest().level.sectors[0].ambient
                : window.documentStateForTest().level.sectors[0].extraLight;
            check(undone
                    && restored == ColorF{ 1.0f, 2.0f, 3.0f, 0.0f },
                ambient
                    ? "sector Ambient same-mean channel normalization is undoable"
                    : "sector Extra same-mean channel normalization is undoable");
        };
    runClassicSameMeanChannelChange(
        true,
        "sector Ambient records a channel-changing edit even when scalar mean is unchanged");
    runClassicSameMeanChannelChange(
        false,
        "sector Extra records a channel-changing edit even when scalar mean is unchanged");

    MainWindow extraWindow{};
    extraWindow.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState extraState{};
    extraState.level.kind = ProjectType::JKDF2;
    extraState.level.layers.emplace_back("Layer0");
    extraState.level.sectors.resize(2);
    for (Sector& extraSector : extraState.level.sectors)
    {
        extraSector.extraLight = ColorF{ 1.25f, 1.25f, 1.25f, 0.0f };
    }
    extraState.selection.sectors.insert(0);
    extraState.selection.sectors.insert(1);
    extraWindow.setDocumentStateForTest(std::move(extraState));

    ui::ScrubFloatEdit* extraEditor =
        findExtraIntensityEditorForTest(extraWindow);
    check(extraEditor != nullptr
            && extraEditor->value() == static_cast<double>(1.25f),
        "sector extra-light editor is found under the nested Lighting row");
    if (extraEditor)
    {
        extraEditor->valueCommitted(-7.5);
        QApplication::processEvents();
    }
    const auto extraValueIs = [&](const float expected) {
        const auto& sectors = extraWindow.documentStateForTest().level.sectors;
        return sectors.size() == 2
            && sectors[0].extraLight.r == expected
            && sectors[0].extraLight.g == expected
            && sectors[0].extraLight.b == expected
            && sectors[0].extraLight.a == 0.0f
            && sectors[1].extraLight == sectors[0].extraLight;
    };
    check(extraValueIs(-7.5f) && extraWindow.undoDepthForTest() == 1,
        "sector extra-light edit applies one signed value to every selected sector in one undo step");
    check(extraWindow.undoForTest()
            && extraValueIs(1.25f)
            && extraWindow.redoForTest()
            && extraValueIs(-7.5f),
        "sector extra-light edit preserves exact values through undo and redo");

    const ColorF extraBeforeInvalid = extraWindow.documentStateForTest()
        .level.sectors[0].extraLight;
    const std::size_t extraHistoryBeforeInvalid =
        extraWindow.undoDepthForTest();
    bool extraInvalidValuesRejected = true;
    for (const double invalidValue : invalidValues)
    {
        extraEditor = findExtraIntensityEditorForTest(extraWindow);
        if (!extraEditor)
        {
            extraInvalidValuesRejected = false;
            break;
        }
        extraEditor->valueCommitted(invalidValue);
        QApplication::processEvents();
        const ColorF current = extraWindow.documentStateForTest()
            .level.sectors[0].extraLight;
        extraInvalidValuesRejected = extraInvalidValuesRejected
            && current == extraBeforeInvalid
            && extraWindow.undoDepthForTest() == extraHistoryBeforeInvalid;
    }
    check(extraInvalidValuesRejected,
        "sector extra-light rejects nonfinite and out-of-float-range commits without changing RGB, alpha, or history");
    const auto beginScrubDrag = [](ui::ScrubFloatEdit& editor) {
        QMouseEvent press(
            QEvent::MouseButtonPress,
            QPointF(2.0, 2.0), QPointF(100.0, 2.0),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(editor.lineEdit(), &press);

        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(62.0, 2.0), QPointF(160.0, 2.0),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(editor.lineEdit(), &move);
    };
    const auto endScrubDrag = [](ui::ScrubFloatEdit& editor,
                                 const bool cancel) {
        if (cancel)
        {
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(editor.lineEdit(), &escape);
        }
        else
        {
            QMouseEvent release(
                QEvent::MouseButtonRelease,
                QPointF(62.0, 2.0), QPointF(160.0, 2.0),
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(editor.lineEdit(), &release);
        }
    };

    const auto runClassicLiveSectorScrub =
        [&](const bool ambient, const bool cancel) {
            MainWindow window{};
            window.setAttribute(Qt::WA_DontShowOnScreen);
            editor::DocumentState state{};
            state.level.kind = ProjectType::JKDF2;
            state.level.layers.emplace_back("Layer0");
            state.level.sectors.resize(2);
            Sector& sector = state.level.sectors[0];
            Sector& otherSector = state.level.sectors[1];
            sector.ambient = ColorF{ 0.375f, 0.375f, 0.375f, 0.0f };
            sector.extraLight = ColorF{ -0.625f, -0.625f, -0.625f, 0.0f };
            otherSector.ambient = ColorF{ 1.375f, 1.375f, 1.375f, 0.0f };
            otherSector.extraLight =
                ColorF{ -1.625f, -1.625f, -1.625f, 0.0f };
            sector.vertices = {
                Vertex{ 0.0, 0.0, 0.0, 0 },
                Vertex{ 1.0, 0.0, 0.0, 0 },
                Vertex{ 0.0, 1.0, 0.0, 0 },
            };
            Surface& surface = sector.surfaces.emplace_back();
            surface.vertices = { 0, 1, 2 };
            surface.texVertices = {
                TexVertex{ 0.0f, 0.0f, ColorF{} },
                TexVertex{ 1.0f, 0.0f, ColorF{} },
                TexVertex{ 0.0f, 1.0f, ColorF{} },
            };
            state.selection.sectors.insert(0);
            state.selection.sectors.insert(1);
            window.setDocumentStateForTest(std::move(state));

            ui::ScrubFloatEdit* scrub = ambient
                ? findAmbientIntensityEditorForTest(window)
                : findExtraIntensityEditorForTest(window);
            check(scrub != nullptr,
                ambient
                    ? "classic sector Ambient exposes a live scrub editor"
                    : "classic sector Extra exposes a live scrub editor");
            GlView* viewport = window.findChild<GlView*>();
            if (!scrub || !viewport)
            {
                return;
            }

            const auto getValue = [&](const std::size_t index) {
                return ambient
                    ? window.documentStateForTest().level.sectors[index].ambient
                    : window.documentStateForTest().level.sectors[index].extraLight;
            };
            const ColorF before = getValue(0);
            const ColorF otherBefore = getValue(1);
            const double startValue = scrub->value();
            const std::uint64_t rebuildsBefore =
                viewport->meshRebuildCountForTest();
            beginScrubDrag(*scrub);
            const ColorF during = getValue(0);
            const ColorF otherDuring = getValue(1);
            if (cancel)
            {
                endScrubDrag(*scrub, true);
                check(getValue(0) == before && getValue(1) == otherBefore
                        && window.undoDepthForTest() == 0u,
                    ambient
                        ? "canceling heterogeneous sector Ambient scrub restores each exact authored value without history"
                        : "canceling heterogeneous sector Extra scrub restores each exact authored value without history");
            }
            else
            {
                check(during.r != before.r
                        && close(during.r, startValue + 0.1, 1.0e-6)
                        && otherDuring == during
                        && viewport->meshRebuildCountForTest() > rebuildsBefore,
                    ambient
                        ? "sector Ambient values and viewport update during drag before release"
                        : "sector Extra values and viewport update during drag before release");
                check(findIntensityEditorForTest(
                          window, ambient ? QStringLiteral("Ambient")
                                          : QStringLiteral("Extra")) == scrub,
                    "live sector lighting drag keeps its property widget alive until release");
                endScrubDrag(*scrub, false);
                const ColorF after = getValue(0);
                const ColorF otherAfter = getValue(1);
                check(window.undoDepthForTest() == 1u
                        && window.undoForTest()
                        && getValue(0) == before
                        && getValue(1) == otherBefore
                        && window.redoForTest()
                        && getValue(0) == after
                        && getValue(1) == otherAfter,
                    ambient
                        ? "heterogeneous sector Ambient scrub is one exact undoable gesture"
                        : "heterogeneous sector Extra scrub is one exact undoable gesture");
            }
        };
    runClassicLiveSectorScrub(true, false);
    runClassicLiveSectorScrub(false, false);
    runClassicLiveSectorScrub(true, true);
    runClassicLiveSectorScrub(false, true);

    MainWindow surfaceScrubWindow{};
    surfaceScrubWindow.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState surfaceScrubState{};
    surfaceScrubState.level.kind = ProjectType::JKDF2;
    surfaceScrubState.level.layers.emplace_back("Layer0");
    surfaceScrubState.level.sectors.resize(2);
    const auto addSurfaceForScrub = [](Sector &sector, const ColorF extraLight) {
        sector.vertices = {
            Vertex{ 0.0, 0.0, 0.0, 0 },
            Vertex{ 1.0, 0.0, 0.0, 0 },
            Vertex{ 0.0, 1.0, 0.0, 0 },
        };
        Surface &surface = sector.surfaces.emplace_back();
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            TexVertex{ 0.0f, 0.0f, ColorF{} },
            TexVertex{ 1.0f, 0.0f, ColorF{} },
            TexVertex{ 0.0f, 1.0f, ColorF{} },
        };
        surface.extraLight = extraLight;
    };
    addSurfaceForScrub(
        surfaceScrubState.level.sectors[0],
        ColorF{ 0.1875f, 0.1875f, 0.1875f, 0.25f });
    addSurfaceForScrub(
        surfaceScrubState.level.sectors[1],
        ColorF{ 0.4375f, 0.4375f, 0.4375f, 0.75f });
    surfaceScrubState.selection.surfaces.insert(SurfaceRef{ 0, 0 });
    surfaceScrubState.selection.surfaces.insert(SurfaceRef{ 1, 0 });
    surfaceScrubWindow.setDocumentStateForTest(std::move(surfaceScrubState));

    ui::ScrubFloatEdit* surfaceScrub =
        surfaceScrubWindow.findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("surfaceExtraLightIntensityEdit"));
    GlView* surfaceScrubViewport = surfaceScrubWindow.findChild<GlView*>();
    check(surfaceScrub != nullptr && surfaceScrubViewport != nullptr,
        "classic surface Extra Light exposes its scrub editor and viewport");
    if (surfaceScrub && surfaceScrubViewport)
    {
        const auto getSurfaceExtra = [&](const std::size_t index) {
            return surfaceScrubWindow.documentStateForTest()
                .level.sectors[index].surfaces[0].extraLight;
        };
        const ColorF before = getSurfaceExtra(0);
        const ColorF otherBefore = getSurfaceExtra(1);
        const double authoredEditorValue = surfaceScrub->value();
        beginScrubDrag(*surfaceScrub);
        const ColorF canceledPreview = getSurfaceExtra(0);
        const ColorF otherCanceledPreview = getSurfaceExtra(1);
        endScrubDrag(*surfaceScrub, true);
        check(canceledPreview != before
                && otherCanceledPreview != otherBefore
                && getSurfaceExtra(0) == before
                && getSurfaceExtra(1) == otherBefore
                && surfaceScrubWindow.undoDepthForTest() == 0u,
            "canceling heterogeneous surface Extra Light scrub restores exact RGB and alpha without history");
        check(surfaceScrub->value() == authoredEditorValue,
            "surface Extra Light editor returns to its exact authored value on cancel");

        const double startValue = surfaceScrub->value();
        const std::uint64_t rebuildsBefore =
            surfaceScrubViewport->meshRebuildCountForTest();
        if (surfaceScrub)
        {
            beginScrubDrag(*surfaceScrub);
            const ColorF during = getSurfaceExtra(0);
            const ColorF otherDuring = getSurfaceExtra(1);
            check(during.r != before.r
                    && close(during.r, startValue + 0.1, 1.0e-6)
                    && otherDuring.r == during.r
                    && during.a == before.a
                    && otherDuring.a == otherBefore.a
                    && surfaceScrubViewport->meshRebuildCountForTest() > rebuildsBefore,
                "surface Extra Light values and viewport update during drag before release");
            check(surfaceScrubWindow.findChild<ui::ScrubFloatEdit*>(
                      QStringLiteral("surfaceExtraLightIntensityEdit"))
                      == surfaceScrub,
                "surface Extra Light live drag keeps its property widget alive until release");
            endScrubDrag(*surfaceScrub, false);
            const ColorF after = getSurfaceExtra(0);
            const ColorF otherAfter = getSurfaceExtra(1);
            check(surfaceScrubWindow.undoDepthForTest() == 1u
                    && surfaceScrubWindow.undoForTest()
                    && getSurfaceExtra(0) == before
                    && getSurfaceExtra(1) == otherBefore
                    && surfaceScrubWindow.redoForTest()
                    && getSurfaceExtra(0) == after
                    && getSurfaceExtra(1) == otherAfter,
                "heterogeneous surface Extra Light scrub preserves RGB and alpha through one undo/redo gesture");
        }
    }
    MainWindow stepperWindow{};
    stepperWindow.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState stepperState{};
    stepperState.level.kind = ProjectType::JKDF2;
    stepperState.level.layers.emplace_back("Layer0");
    Sector& stepperSector = stepperState.level.sectors.emplace_back();
    stepperSector.ambient = ColorF{ 0.625f, 0.625f, 0.625f, 0.0f };
    stepperState.selection.sectors.insert(0);
    stepperWindow.setDocumentStateForTest(std::move(stepperState));
    ui::ScrubFloatEdit* stepperEditor =
        findAmbientIntensityEditorForTest(stepperWindow);
    check(stepperEditor != nullptr,
        "classic sector Ambient exposes a scrub stepper");
    if (stepperEditor)
    {
        const ColorF before = stepperWindow.documentStateForTest()
            .level.sectors[0].ambient;
        QToolButton* decrement = nullptr;
        QToolButton* increment = nullptr;
        for (QToolButton* button : stepperEditor->findChildren<QToolButton*>())
        {
            if (button->text() == QStringLiteral("<")) decrement = button;
            if (button->text() == QStringLiteral(">")) increment = button;
        }
        check(decrement && increment && decrement->autoRepeat()
                && increment->autoRepeat(),
            "lighting stepper arrows retain Qt hold-to-repeat configuration");
        if (decrement && increment)
        {
            for (int repeat = 0; repeat < 4; ++repeat)
            {
                QMetaObject::invokeMethod(
                    decrement, "pressed", Qt::DirectConnection);
                check(findAmbientIntensityEditorForTest(stepperWindow)
                        == stepperEditor,
                    "holding the lighting stepper does not recreate its active widget");
            }
            QMetaObject::invokeMethod(
                decrement, "released", Qt::DirectConnection);
            const ColorF after = stepperWindow.documentStateForTest()
                .level.sectors[0].ambient;
            check(close(after.r, static_cast<double>(before.r) - 0.04, 1.0e-6)
                    && stepperWindow.undoDepthForTest() == 1u
                    && stepperWindow.undoForTest()
                    && stepperWindow.documentStateForTest()
                           .level.sectors[0].ambient == before
                    && stepperWindow.redoForTest()
                    && stepperWindow.documentStateForTest()
                           .level.sectors[0].ambient == after,
                "held lighting stepper repeats within one exact undoable gesture");
        }
    }

}

} // namespace sed::app_tests
