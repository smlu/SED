#include "test_common.hpp"

#include "main_window/main_window.hpp"
#include "app_settings.hpp"
#include "gl_view.hpp"
#include "resource_release_test_fixtures.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QCborMap>
#include <QCborValue>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QStackedWidget>
#include <QSettings>
#include <QToolButton>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sed
{
std::vector<ThingTemplate> templateEditorTemplatesForTest(QDialog* dialog);
}

namespace sed::app_tests
{
namespace
{

TplValue stringField(const std::string& name, const std::string& value)
{
    TplValue field{};
    field.name = name;
    field.type = ValueType::str;
    field.stringValue = value;
    return field;
}

TplValue floatField(const std::string& name, const double value)
{
    TplValue field{};
    field.name = name;
    field.type = ValueType::floating;
    field.floatValue = value;
    return field;
}

TplValue integerField(const std::string& name, const std::int32_t value)
{
    TplValue field{};
    field.name = name;
    field.type = ValueType::integer;
    field.intValue = value;
    return field;
}

bool editTemplateParameter(
    QDialog& editor,
    const QString& parameterName,
    const QString& value,
    const bool commitWithReturnKey = false)
{
    auto* table = editor.findChild<QTableWidget*>(QStringLiteral("templateParametersTable"));
    if (!table)
    {
        return false;
    }

    int row = -1;
    for (int index = 0; index < table->rowCount(); ++index)
    {
        const QTableWidgetItem* item = table->item(index, 0);
        if (item && item->text().compare(parameterName, Qt::CaseInsensitive) == 0)
        {
            row = index;
            break;
        }
    }
    if (row < 0)
    {
        return false;
    }

    table->selectRow(row);
    QWidget* valueCell = table->cellWidget(row, 2);
    auto* valueEdit = valueCell
        ? valueCell->findChild<QLineEdit*>(QStringLiteral("templateParameterInlineValueEdit"))
        : nullptr;
    if (!valueEdit)
    {
        return false;
    }
    valueEdit->setText(value);
    if (commitWithReturnKey)
    {
        QKeyEvent returnKey(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QCoreApplication::sendEvent(valueEdit, &returnKey);
    }
    else
    {
        (void)QMetaObject::invokeMethod(valueEdit, "editingFinished", Qt::DirectConnection);
    }
    // Return first schedules the editingFinished delivery, which schedules a
    // guarded table refresh after the edit signal stack has unwound.
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    const QTableWidgetItem* committed = table->item(row, 2);
    return committed && committed->text() == value;
}

bool editTemplateParameterThroughUi(
    QDialog& editor,
    const QString& parameterName,
    const QString& value,
    const bool commitWithReturnKey)
{
    auto* table = editor.findChild<QTableWidget*>(QStringLiteral("templateParametersTable"));
    if (!table)
    {
        return false;
    }

    int row = -1;
    for (int index = 0; index < table->rowCount(); ++index)
    {
        const QTableWidgetItem* item = table->item(index, 0);
        if (item && item->text().compare(parameterName, Qt::CaseInsensitive) == 0)
        {
            row = index;
            break;
        }
    }
    if (row < 0)
    {
        return false;
    }

    table->selectRow(row);
    QWidget* valueCell = table->cellWidget(row, 2);
    auto* valueEdit = valueCell
        ? valueCell->findChild<QLineEdit*>(QStringLiteral("templateParameterInlineValueEdit"))
        : nullptr;
    if (!valueEdit)
    {
        return false;
    }

    valueEdit->setFocus(Qt::OtherFocusReason);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    if (!valueEdit->hasFocus())
    {
        return false;
    }

    valueEdit->selectAll();
    for (const QChar character : value)
    {
        const Qt::Key key = static_cast<Qt::Key>(character.unicode());
        if ((character >= QLatin1Char('0') && character <= QLatin1Char('9'))
            || character == QLatin1Char('.') || character == QLatin1Char('-'))
        {
            QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(character));
            QApplication::sendEvent(valueEdit, &press);
            QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(character));
            QApplication::sendEvent(valueEdit, &release);
        }
        else
        {
            return false;
        }
    }

    if (commitWithReturnKey)
    {
        QKeyEvent enterPress(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(valueEdit, &enterPress);
        QKeyEvent enterRelease(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(valueEdit, &enterRelease);
    }
    else
    {
        // Changing focus exercises the same focusOut/editingFinished path as
        // clicking another control after typing in the inline editor.
        valueEdit->clearFocus();
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    for (int index = 0; index < table->rowCount(); ++index)
    {
        const QTableWidgetItem* item = table->item(index, 0);
        const QTableWidgetItem* committed = table->item(index, 2);
        if (item && committed
            && item->text().compare(parameterName, Qt::CaseInsensitive) == 0)
        {
            return committed->text() == value;
        }
    }
    return false;
}

std::optional<std::array<double, 6>> guideBounds(GlView* view)
{
    return view ? view->thingTemplateCollisionBoundsGuideBoundsForTest(0, true)
                : std::nullopt;
}

} // namespace

void runTemplateCollisionBoundsPreviewTests()
{
    QTemporaryDir resourceDirectory{};
    check(resourceDirectory.isValid(),
        "Template collision preview fixture has a temporary resource root");
    if (!resourceDirectory.isValid())
    {
        return;
    }

    const std::filesystem::path resourceRoot(resourceDirectory.path().toStdWString());
    const std::filesystem::path modelPath = resourceRoot / "template_collision_preview.3do";
    const QByteArray model = fixtures::makeDiskChangeModelFixture(
        "unused.mat", "template collision preview fixture", 1.0);
    {
        std::ofstream output(modelPath, std::ios::binary);
        output.write(model.constData(), model.size());
    }
    check(std::filesystem::exists(modelPath),
        "Template collision preview fixture writes a synthetic 3DO model");

    assets::ResourceLocator resources{};
    resources.addRoot(resourceRoot);

    ThingTemplate authored{};
    authored.name = "collision_preview_template";
    authored.parent = "none";
    authored.values = {
        stringField("model3d", "template_collision_preview.3do"),
        integerField("collide", 1),
        floatField("size", 2.5),
        floatField("collwidth", 1.25),
        floatField("collheight", 3.5),
    };

    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    std::unique_ptr<QDialog> editor(window.createTemplateEditorDialogForTest(
        { authored }, ProjectType::IJIM, {}, resources));
    auto* preview = editor
        ? editor->findChild<GlView*>(QStringLiteral("templatePreviewView"))
        : nullptr;
    auto* legend = editor
        ? editor->findChild<QLabel*>(QStringLiteral("templatePreviewCollisionLegend"))
        : nullptr;
    auto* collisionToggle = editor
        ? editor->findChild<QToolButton*>(QStringLiteral("templatePreviewCollisionToggle"))
        : nullptr;
    check(preview != nullptr && legend != nullptr && collisionToggle != nullptr,
        "Template Editor exposes its preview view, collision toggle, and legend");
    if (!preview || !legend || !collisionToggle)
    {
        return;
    }

    check(collisionToggle->text().isEmpty()
            && !collisionToggle->icon().isNull()
            && collisionToggle->isChecked()
            && !collisionToggle->isHidden(),
        "3D template preview shows a checked icon-only collision toggle by default");
    check(preview->showThingCollisionShapes()
            && preview->showTemplateCollisionBoundsGuide()
            && window.glViewForTest()
            && !window.glViewForTest()->showTemplateCollisionBoundsGuide(),
        "Collision overlays are enabled only on the IJIM Template Editor preview");
    check(legend->text().contains(QStringLiteral("<b>collide</b>"))
            && legend->text().contains(QStringLiteral("<b>size</b>"))
            && legend->text().contains(QStringLiteral("<b>collwidth</b>"))
            && legend->text().contains(QStringLiteral("<b>collheight</b>"))
            && legend->text().contains(QStringLiteral("bounding box"))
            && !legend->isHidden(),
        "Template Editor legend names each collision and IJIM bounds parameter in its guide color");

    collisionToggle->click();
    check(!collisionToggle->isChecked()
            && !preview->showThingCollisionShapes()
            && !preview->showTemplateCollisionBoundsGuide()
            && legend->isHidden(),
        "Turning off collision preview hides both guides and their description");
    collisionToggle->click();
    check(collisionToggle->isChecked()
            && preview->showThingCollisionShapes()
            && preview->showTemplateCollisionBoundsGuide()
            && !legend->isHidden(),
        "Turning collision preview back on restores the guides and concise description");

    const auto initialBounds = guideBounds(preview);
    check(initialBounds.has_value()
            && close((*initialBounds)[0], -1.25)
            && close((*initialBounds)[1], -1.25)
            && close((*initialBounds)[2], -3.5)
            && close((*initialBounds)[3], 1.25)
            && close((*initialBounds)[4], 1.25)
            && close((*initialBounds)[5], 3.5),
        "Template preview renders inherited/authored width and height at their exact extents");
    const std::optional<double> initialCollisionExtent =
        preview->thingCollisionOverlayExtentForTest(0, true);
    check(initialCollisionExtent && close(*initialCollisionExtent, 2.5, 1.0e-5),
        "Template preview renders the effective collide/size shape independently of IJIM bounds guide");

    auto* parameterTable = editor->findChild<QTableWidget*>(
        QStringLiteral("templateParametersTable"));
    int collideRow = -1;
    for (int row = 0; parameterTable && row < parameterTable->rowCount(); ++row)
    {
        const QTableWidgetItem* item = parameterTable->item(row, 0);
        if (item && item->text() == QStringLiteral("collide"))
        {
            collideRow = row;
            break;
        }
    }
    QWidget* collideCell = parameterTable && collideRow >= 0
        ? parameterTable->cellWidget(collideRow, 2)
        : nullptr;
    auto* collideEdit = collideCell
        ? collideCell->findChild<QLineEdit*>(
              QStringLiteral("templateParameterInlineValueEdit"))
        : nullptr;
    if (collideEdit)
    {
        collideEdit->setText(QStringLiteral("invalid"));
        (void)QMetaObject::invokeMethod(
            collideEdit, "editingFinished", Qt::DirectConnection);
    }
    QCoreApplication::processEvents();
    QWidget* restoredCollideCell = parameterTable && collideRow >= 0
        ? parameterTable->cellWidget(collideRow, 2)
        : nullptr;
    auto* restoredCollideEdit = restoredCollideCell
        ? restoredCollideCell->findChild<QLineEdit*>(
              QStringLiteral("templateParameterInlineValueEdit"))
        : nullptr;
    check(restoredCollideEdit && restoredCollideEdit->text() == QStringLiteral("1")
            && parameterTable->item(collideRow, 2)->text() == QStringLiteral("1")
            && preview->thingCollisionOverlayExtentForTest(0, true).has_value(),
        "Invalid inline typed values restore the previous token and leave collision preview state unchanged");

    check(editTemplateParameter(*editor, QStringLiteral("collwidth"), QStringLiteral("2.25")),
        "Template Editor can edit authored IJIM width from the parameter table");
    const auto editedWidth = guideBounds(preview);
    check(editedWidth.has_value()
            && close((*editedWidth)[0], -2.25)
            && close((*editedWidth)[3], 2.25)
            && close((*editedWidth)[2], -3.5)
            && close((*editedWidth)[5], 3.5),
        "Editing collwidth immediately updates the world-axis-aligned preview box without changing height");

    check(editTemplateParameter(*editor, QStringLiteral("collheight"), QStringLiteral("4.75")),
        "Template Editor can edit authored IJIM height from the parameter table");
    const auto editedHeight = guideBounds(preview);
    check(editedHeight.has_value()
            && close((*editedHeight)[0], -2.25)
            && close((*editedHeight)[3], 2.25)
            && close((*editedHeight)[2], -4.75)
            && close((*editedHeight)[5], 4.75),
        "Editing collheight immediately updates the world-axis-aligned preview box without changing width");

    check(editTemplateParameter(*editor, QStringLiteral("collheight"), QStringLiteral("0")),
        "Template Editor accepts zero authored height and uses the engine fallback");
    check(!guideBounds(preview).has_value()
            && preview->thingCollisionOverlayExtentForTest(0, true).has_value(),
        "IJIM parameter guide disappears if either dimension is zero while effective collision stays independent");
    check(editTemplateParameter(*editor, QStringLiteral("collheight"), QStringLiteral("4.75"))
            && guideBounds(preview).has_value(),
        "Restoring both positive authored dimensions restores the complete IJIM parameter box");

    check(editTemplateParameter(*editor, QStringLiteral("size"), QStringLiteral("3.75")),
        "Template Editor can edit effective collision size from the parameter table");
    const std::optional<double> editedCollisionExtent =
        preview->thingCollisionOverlayExtentForTest(0, true);
    check(editedCollisionExtent && close(*editedCollisionExtent, 3.75, 1.0e-5),
        "Editing size immediately updates the effective collide sphere preview");

    check(editTemplateParameter(*editor, QStringLiteral("collide"), QStringLiteral("0")),
        "Template Editor can disable the effective collision preview");
    check(!preview->thingCollisionOverlayExtentForTest(0, true)
            && guideBounds(preview).has_value(),
        "Disabling collide hides the effective collision shape but retains authored IJIM dimension guides");
    editor.reset();
    ThingTemplate collisionOnly{};
    collisionOnly.name = "collision_only_template";
    collisionOnly.parent = "none";
    collisionOnly.values = {
        integerField("collide", 1),
        floatField("size", 2.0),
        floatField("collwidth", 0.5),
        floatField("collheight", 1.5),
    };
    std::unique_ptr<QDialog> collisionOnlyEditor(window.createTemplateEditorDialogForTest(
        { collisionOnly }, ProjectType::IJIM, {}, resources));
    auto* collisionOnlyPreview = collisionOnlyEditor
        ? collisionOnlyEditor->findChild<GlView*>(QStringLiteral("templatePreviewView"))
        : nullptr;
    auto* collisionOnlyStack = collisionOnlyEditor
        ? collisionOnlyEditor->findChild<QStackedWidget*>(QStringLiteral("templatePreviewStack"))
        : nullptr;
    auto* collisionOnlyLegend = collisionOnlyEditor
        ? collisionOnlyEditor->findChild<QLabel*>(QStringLiteral("templatePreviewCollisionLegend"))
        : nullptr;
    auto* collisionOnlyToggle = collisionOnlyEditor
        ? collisionOnlyEditor->findChild<QToolButton*>(QStringLiteral("templatePreviewCollisionToggle"))
        : nullptr;
    check(collisionOnlyPreview && collisionOnlyStack && collisionOnlyLegend && collisionOnlyToggle
            && collisionOnlyStack->currentWidget() == collisionOnlyPreview
            && collisionOnlyToggle->isChecked()
            && collisionOnlyLegend->text().contains(QStringLiteral("Green"))
            && collisionOnlyPreview->thingCollisionOverlayExtentForTest(0, true).has_value()
            && guideBounds(collisionOnlyPreview).has_value(),
        "Templates without a model or sprite show a labeled collision-only viewport preview");
    if (collisionOnlyPreview && collisionOnlyLegend && collisionOnlyToggle)
    {
        collisionOnlyToggle->click();
        check(!collisionOnlyPreview->showThingCollisionShapes()
                && !collisionOnlyPreview->showTemplateCollisionBoundsGuide()
                && collisionOnlyLegend->isHidden(),
            "The no-model collision preview toggle hides both guide types and the legend");
        collisionOnlyToggle->click();
        check(collisionOnlyPreview->showThingCollisionShapes()
                && collisionOnlyPreview->showTemplateCollisionBoundsGuide()
                && !collisionOnlyLegend->isHidden(),
            "The no-model collision preview toggle restores both guide types and the legend");
    }

    ThingTemplate tinyCollisionOnly{};
    tinyCollisionOnly.name = "tiny_collision_only_template";
    tinyCollisionOnly.parent = "none";
    tinyCollisionOnly.bbox.Set(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0);
    tinyCollisionOnly.values = {
        integerField("collide", 1),
        floatField("size", 0.04),
    };
    std::unique_ptr<QDialog> tinyCollisionEditor(window.createTemplateEditorDialogForTest(
        { tinyCollisionOnly }, ProjectType::MOTS, {}, resources));
    auto* tinyCollisionPreview = tinyCollisionEditor
        ? tinyCollisionEditor->findChild<GlView*>(QStringLiteral("templatePreviewView"))
        : nullptr;
    auto* tinyCollisionToggle = tinyCollisionEditor
        ? tinyCollisionEditor->findChild<QToolButton*>(QStringLiteral("templatePreviewCollisionToggle"))
        : nullptr;
    check(tinyCollisionPreview && tinyCollisionToggle
            && tinyCollisionToggle->isChecked()
            && close(tinyCollisionPreview->thingCollisionOverlayExtentForTest(0, true).value_or(0.0), 0.04, 1.0e-5),
        "Tiny collision-only template uses its effective size while keeping the collision preview enabled");
    if (tinyCollisionEditor && tinyCollisionPreview)
    {
        tinyCollisionEditor->setAttribute(Qt::WA_DontShowOnScreen);
        tinyCollisionEditor->resize(900, 700);
        tinyCollisionEditor->show();
        QCoreApplication::processEvents();
        // The dialog starts a 90 ms resize-refit timer when the preview becomes
        // visible. Wait for that callback so this check covers the final fit,
        // not only the synchronous fit performed during template loading.
        QEventLoop resizeRefitLoop{};
        QTimer::singleShot(120, &resizeRefitLoop, [&resizeRefitLoop]()
            {
                resizeRefitLoop.quit();
            });
        resizeRefitLoop.exec();
        QCoreApplication::processEvents();
        const auto center = tinyCollisionPreview->projectWorldToViewport(QVector3D(0.0f, 0.0f, 0.0f));
        const auto edge = tinyCollisionPreview->projectWorldToViewport(QVector3D(0.04f, 0.0f, 0.0f));
        const int viewportHeight = tinyCollisionPreview->height();
        const double projectedRadius = center && edge
            ? std::hypot(center->x() - edge->x(), center->y() - edge->y())
            : 0.0;
        const bool effectivelyFramed = viewportHeight > 0
            && projectedRadius > static_cast<double>(viewportHeight) * 0.15;
        if (!effectivelyFramed)
        {
            const QVector3D eye = tinyCollisionPreview->cameraEye();
            const QVector3D target = tinyCollisionPreview->cameraTarget();
            std::cerr << "Tiny collision-only framing: projectedRadius=" << projectedRadius
                      << " viewport=" << tinyCollisionPreview->width() << 'x' << viewportHeight
                      << " centerValid=" << static_cast<bool>(center)
                      << " edgeValid=" << static_cast<bool>(edge)
                      << " projection=" << static_cast<int>(tinyCollisionPreview->cameraProjectionType())
                      << " orthoScale=" << tinyCollisionPreview->cameraOrthoScale()
                      << " orbitDistance=" << tinyCollisionPreview->cameraOrbitDistance()
                      << " eye=(" << eye.x() << ',' << eye.y() << ',' << eye.z() << ')'
                      << " target=(" << target.x() << ',' << target.y() << ',' << target.z() << ")\n";
        }
        check(effectivelyFramed,
            "Tiny collision-only preview frames the effective size instead of an unrelated template BBox");
        tinyCollisionEditor->hide();
    }

    ThingTemplate motsTreeTemplate{};
    motsTreeTemplate.name = "+carbonitechar";
    motsTreeTemplate.parent = "mots_tree_middle";
    motsTreeTemplate.values = {
        integerField("collide", 2),
    };
    ThingTemplate motsTreeMiddle{};
    motsTreeMiddle.name = "mots_tree_middle";
    motsTreeMiddle.parent = "mots_tree_base";
    motsTreeMiddle.values = {
        floatField("treesize", 0.5),
    };
    ThingTemplate motsTreeBase{};
    motsTreeBase.name = "mots_tree_base";
    motsTreeBase.parent = "none";
    motsTreeBase.values = {
        stringField("model3d", "template_collision_preview.3do"),
        integerField("collide", 1),
        floatField("size", 1.0),
    };
    std::unique_ptr<QDialog> motsTreeEditor(window.createTemplateEditorDialogForTest(
        { motsTreeTemplate, motsTreeMiddle, motsTreeBase }, ProjectType::MOTS, {}, resources, 1.0f));
    auto* motsTreePreview = motsTreeEditor
        ? motsTreeEditor->findChild<GlView*>(QStringLiteral("templatePreviewView"))
        : nullptr;
    auto* motsTreeLegend = motsTreeEditor
        ? motsTreeEditor->findChild<QLabel*>(QStringLiteral("templatePreviewCollisionLegend"))
        : nullptr;
    auto* motsTreeToggle = motsTreeEditor
        ? motsTreeEditor->findChild<QToolButton*>(QStringLiteral("templatePreviewCollisionToggle"))
        : nullptr;
    const std::optional<double> motsTreeExtent = motsTreePreview
        ? motsTreePreview->thingCollisionOverlayExtentForTest(0, true)
        : std::nullopt;
    const std::optional<double> motsBroadPhaseExtent = motsTreePreview
        ? motsTreePreview->thingBroadPhaseTreeGuideExtentForTest(0, true)
        : std::nullopt;
    check(motsTreePreview && motsTreeLegend && motsTreeToggle
            && motsTreeToggle->isChecked()
            && motsTreePreview->motsTreeCollisionPreviewMeshRadiusFactor() == 1.0f
            && motsTreeExtent && close(*motsTreeExtent, 4.0, 1.0e-5)
            && motsBroadPhaseExtent && close(*motsBroadPhaseExtent, 0.5, 1.0e-5)
            && motsTreeLegend->text().contains(QStringLiteral("<b>Collision Tree</b>"))
            && motsTreeLegend->text().contains(QStringLiteral("<b>treesize</b>"))
            && motsTreeLegend->text().contains(QStringLiteral("Broad-phase"))
            && motsTreeLegend->text().contains(QStringLiteral("default hierarchy pose")),
        "MotS Template Editor applies the preview radius preference to per-mesh spheres while leaving inherited treesize broad-phase geometry unchanged");
    if (motsTreePreview)
    {
        motsTreePreview->setMotsTreeCollisionPreviewMeshRadiusFactor(0.5f);
    }
    const std::optional<double> halfScaleTreeExtent = motsTreePreview
        ? motsTreePreview->thingCollisionOverlayExtentForTest(0, true)
        : std::nullopt;
    const std::optional<double> halfScaleBroadPhaseExtent = motsTreePreview
        ? motsTreePreview->thingBroadPhaseTreeGuideExtentForTest(0, true)
        : std::nullopt;
    check(halfScaleTreeExtent && close(*halfScaleTreeExtent, 2.0, 1.0e-5)
            && halfScaleBroadPhaseExtent && close(*halfScaleBroadPhaseExtent, 0.5, 1.0e-5),
        "Changing the MotS preview factor rebuilds mesh spheres immediately without scaling treesize");
    if (motsTreePreview)
    {
        motsTreePreview->setMotsTreeCollisionPreviewMeshRadiusFactor(1.0f);
    }
    if (motsTreeEditor)
    {
        motsTreeEditor->setAttribute(Qt::WA_DontShowOnScreen);
        motsTreeEditor->resize(1000, 760);
        motsTreeEditor->show();
        motsTreeEditor->activateWindow();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    const bool firstTreeSizeEdit = motsTreeEditor
        && editTemplateParameterThroughUi(
            *motsTreeEditor, QStringLiteral("treesize"), QStringLiteral("0.2"), true);
    const std::optional<double> firstBroadPhaseExtent = firstTreeSizeEdit && motsTreePreview
        ? motsTreePreview->thingBroadPhaseTreeGuideExtentForTest(0, true)
        : std::nullopt;
    const bool secondTreeSizeEdit = firstTreeSizeEdit
        && editTemplateParameterThroughUi(
            *motsTreeEditor, QStringLiteral("treesize"), QStringLiteral("0.5"), false);
    const std::optional<double> secondBroadPhaseExtent = secondTreeSizeEdit && motsTreePreview
        ? motsTreePreview->thingBroadPhaseTreeGuideExtentForTest(0, true)
        : std::nullopt;
    check(firstTreeSizeEdit && firstBroadPhaseExtent
            && close(*firstBroadPhaseExtent, 0.2, 1.0e-5)
            && secondTreeSizeEdit && secondBroadPhaseExtent
            && close(*secondBroadPhaseExtent, 0.5, 1.0e-5)
            && motsTreeEditor->isVisible()
            && motsTreeEditor->result() != QDialog::Accepted,
        "Focused MotS treesize editor commits inherited then local values on Enter and focus loss");
    const std::vector<ThingTemplate> editedMotsTreeTemplates = motsTreeEditor
        ? sed::templateEditorTemplatesForTest(motsTreeEditor.get())
        : std::vector<ThingTemplate>{};
    const std::optional<double> editedMotsTreeExtent = motsTreePreview
        ? motsTreePreview->thingCollisionOverlayExtentForTest(0, true)
        : std::nullopt;
    const auto editedCarbonite = std::find_if(
        editedMotsTreeTemplates.begin(), editedMotsTreeTemplates.end(), [](const ThingTemplate& value)
        {
            return value.name == "+carbonitechar";
        });
    const auto carboniteTreeSize = editedCarbonite == editedMotsTreeTemplates.end()
        ? std::vector<TplValue>::const_iterator{}
        : std::find_if(editedCarbonite->values.begin(), editedCarbonite->values.end(), [](const TplValue& value)
            {
                return value.name == "treesize";
            });
    check(secondTreeSizeEdit && editedCarbonite != editedMotsTreeTemplates.end()
            && carboniteTreeSize != editedCarbonite->values.end()
            && carboniteTreeSize->asString() == "0.5"
            && editedMotsTreeExtent && close(*editedMotsTreeExtent, 4.0, 1.0e-5),
        "Repeated treesize commits materialize the inherited child value without changing preference-scaled per-mesh collision geometry");
    if (motsTreeEditor)
    {
        motsTreeEditor->hide();
    }

    ThingTemplate deepestBase{};
    deepestBase.name = "carbonite_depth_0";
    deepestBase.parent = "none";
    deepestBase.values = {
        stringField("model3d", "template_collision_preview.3do"),
        integerField("collide", 1),
        floatField("size", 1.0),
    };
    ThingTemplate depthOne{};
    depthOne.name = "carbonite_depth_1";
    depthOne.parent = deepestBase.name;
    ThingTemplate depthTwo{};
    depthTwo.name = "carbonite_depth_2";
    depthTwo.parent = depthOne.name;
    depthTwo.values.push_back(floatField("size", 2.0));
    ThingTemplate depthThree{};
    depthThree.name = "carbonite_depth_3";
    depthThree.parent = depthTwo.name;
    ThingTemplate depthFour{};
    depthFour.name = "carbonite_depth_4";
    depthFour.parent = depthThree.name;
    ThingTemplate carboniteCharacter{};
    carboniteCharacter.name = "+carbonitechar";
    carboniteCharacter.parent = depthFour.name;
    std::unique_ptr<QDialog> inheritedCollisionEditor(window.createTemplateEditorDialogForTest(
        { carboniteCharacter, depthFour, depthThree, depthTwo, depthOne, deepestBase },
        ProjectType::MOTS,
        {},
        resources));
    auto* inheritedCollisionPreview = inheritedCollisionEditor
        ? inheritedCollisionEditor->findChild<GlView*>(QStringLiteral("templatePreviewView"))
        : nullptr;
    auto* inheritedCollisionTable = inheritedCollisionEditor
        ? inheritedCollisionEditor->findChild<QTableWidget*>(QStringLiteral("templateParametersTable"))
        : nullptr;
    const auto parameterRow = [](const QTableWidget* table, const QString& name)
    {
        for (int row = 0; table && row < table->rowCount(); ++row)
        {
            if (table->item(row, 0)
                && table->item(row, 0)->text().compare(name, Qt::CaseInsensitive) == 0)
            {
                return row;
            }
        }
        return -1;
    };
    const int inheritedCollideRow = parameterRow(
        inheritedCollisionTable, QStringLiteral("collide"));
    const int inheritedSizeRow = parameterRow(
        inheritedCollisionTable, QStringLiteral("size"));
    check(inheritedCollisionPreview && inheritedCollisionTable
            && inheritedCollisionPreview->thingCollisionOverlayExtentForTest(0, true)
            && close(*inheritedCollisionPreview->thingCollisionOverlayExtentForTest(0, true), 2.0, 1.0e-5)
            && inheritedCollideRow >= 0 && inheritedSizeRow >= 0
            && inheritedCollisionTable->item(inheritedCollideRow, 1)
            && inheritedCollisionTable->item(inheritedSizeRow, 1)
            && inheritedCollisionTable->item(inheritedCollideRow, 1)->text()
                == QStringLiteral("Inherited: carbonite_depth_0")
            && inheritedCollisionTable->item(inheritedSizeRow, 1)->text()
                == QStringLiteral("Inherited: carbonite_depth_2"),
        "MotS collision preview and parameter table resolve values through a five-level template inheritance chain");

    const auto selectTemplate = [](QDialog& editor, const QString& name)
    {
        auto* list = editor.findChild<QListWidget*>(QStringLiteral("templateEditorTemplateList"));
        for (int row = 0; list && row < list->count(); ++row)
        {
            const QListWidgetItem* item = list->item(row);
            if (item && item->data(Qt::UserRole).toString().compare(name, Qt::CaseInsensitive) == 0)
            {
                list->setCurrentRow(row);
                QCoreApplication::processEvents();
                return true;
            }
        }
        return false;
    };
    check(inheritedCollisionEditor && inheritedCollisionPreview
            && selectTemplate(*inheritedCollisionEditor, QStringLiteral("carbonite_depth_2"))
            && editTemplateParameter(
                *inheritedCollisionEditor, QStringLiteral("size"), QStringLiteral("2.25"))
            && selectTemplate(*inheritedCollisionEditor, QStringLiteral("+carbonitechar"))
            && inheritedCollisionPreview->thingCollisionOverlayExtentForTest(0, true)
            && close(*inheritedCollisionPreview->thingCollisionOverlayExtentForTest(0, true), 2.25, 1.0e-5),
        "Editing a mid-chain inherited size immediately updates a descendant collision preview");
    check(inheritedCollisionEditor && inheritedCollisionPreview
            && selectTemplate(*inheritedCollisionEditor, QStringLiteral("carbonite_depth_0"))
            && editTemplateParameter(
                *inheritedCollisionEditor, QStringLiteral("collide"), QStringLiteral("0"))
            && selectTemplate(*inheritedCollisionEditor, QStringLiteral("+carbonitechar"))
            && !inheritedCollisionPreview->thingCollisionOverlayExtentForTest(0, true),
        "Editing a root inherited collide value removes the descendant collision preview");
    check(inheritedCollisionEditor && inheritedCollisionPreview
            && selectTemplate(*inheritedCollisionEditor, QStringLiteral("carbonite_depth_0"))
            && editTemplateParameter(
                *inheritedCollisionEditor, QStringLiteral("collide"), QStringLiteral("1"))
            && selectTemplate(*inheritedCollisionEditor, QStringLiteral("+carbonitechar"))
            && inheritedCollisionPreview->thingCollisionOverlayExtentForTest(0, true)
            && close(*inheritedCollisionPreview->thingCollisionOverlayExtentForTest(0, true), 2.25, 1.0e-5),
        "Restoring an ancestor collision parameter restores the descendant preview without losing nearer size overrides");

    ThingTemplate enterTemplate{};
    enterTemplate.name = "enter_edit_template";
    enterTemplate.parent = "none";
    enterTemplate.values = { stringField("size", "1.0") };
    std::unique_ptr<QDialog> enterEditor(window.createTemplateEditorDialogForTest(
        { enterTemplate }, ProjectType::JKDF2, {}, resources));
    auto* enterTable = enterEditor
        ? enterEditor->findChild<QTableWidget*>(QStringLiteral("templateParametersTable"))
        : nullptr;
    const int enterSizeRow = parameterRow(enterTable, QStringLiteral("size"));
    QWidget* enterValueCell = enterTable && enterSizeRow >= 0
        ? enterTable->cellWidget(enterSizeRow, 2)
        : nullptr;
    auto* enterValueEdit = enterValueCell
        ? enterValueCell->findChild<QLineEdit*>(QStringLiteral("templateParameterInlineValueEdit"))
        : nullptr;
    const QString expectedSizeTextTooltip = QStringLiteral(
        "Defined on this template. Stored as text in the template file. The engine interprets recognized parameter names (including numeric values) according to its own rules, and SED preserves the authored text. Collision and movement size.");
    check(enterValueEdit && enterValueEdit->toolTip() == expectedSizeTextTooltip,
        "Numeric-looking string parameters explain their exact authored-text semantics");
    if (enterEditor && enterValueEdit)
    {
        enterEditor->setAttribute(Qt::WA_DontShowOnScreen);
        enterEditor->show();
        QCoreApplication::processEvents();
        enterValueEdit->setFocus();
        enterValueEdit->selectAll();
        const auto sendKey = [enterValueEdit](const Qt::Key key, const QString& text)
        {
            QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
            QApplication::sendEvent(enterValueEdit, &press);
            QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
            QApplication::sendEvent(enterValueEdit, &release);
        };
        sendKey(Qt::Key_2, QStringLiteral("2"));
        sendKey(Qt::Key_Period, QStringLiteral("."));
        sendKey(Qt::Key_5, QStringLiteral("5"));
        QKeyEvent enterPress(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(enterValueEdit, &enterPress);
        QKeyEvent enterRelease(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(enterValueEdit, &enterRelease);
        // Exercise the Return and ensuing focus-loss notifications before the
        // queued editingFinished delivery and guarded table rebuild run.
        enterValueEdit->clearFocus();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    bool enterValueCommittedToTemplate = false;
    if (enterEditor)
    {
        for (const ThingTemplate& editedTemplate : templateEditorTemplatesForTest(enterEditor.get()))
        {
            for (const TplValue& editedValue : editedTemplate.values)
            {
                if (editedValue.name == "size" && editedValue.asString() == "2.5")
                {
                    enterValueCommittedToTemplate = true;
                }
            }
        }
    }
    const int committedSizeRow = parameterRow(enterTable, QStringLiteral("size"));
    QWidget* committedSizeCell = enterTable && committedSizeRow >= 0
        ? enterTable->cellWidget(committedSizeRow, 2)
        : nullptr;
    check(enterEditor && enterEditor->isVisible()
            && enterEditor->result() != QDialog::Accepted
            && enterTable && committedSizeRow >= 0
            && enterTable->item(committedSizeRow, 2)
            && enterTable->item(committedSizeRow, 2)->text() == QStringLiteral("2.5")
            && committedSizeCell
            && committedSizeCell->toolTip() == expectedSizeTextTooltip
            && enterValueCommittedToTemplate,
        "Enter commits the focused Template Editor value field without accepting or closing the editor");
    if (enterEditor)
    {
        enterEditor->hide();
    }

    ThingTemplate bboxOnlyTemplate{};
    bboxOnlyTemplate.name = "bbox_only_template";
    bboxOnlyTemplate.parent = "none";
    bboxOnlyTemplate.bbox.Set(-1.0, -2.0, -0.5, 2.0, 1.0, 3.0);
    std::unique_ptr<QDialog> bboxOnlyEditor(window.createTemplateEditorDialogForTest(
        { bboxOnlyTemplate }, ProjectType::JKDF2, {}, resources));
    auto* bboxOnlyPreview = bboxOnlyEditor
        ? bboxOnlyEditor->findChild<GlView*>(QStringLiteral("templatePreviewView"))
        : nullptr;
    const Level* bboxOnlyPreviewLevel = bboxOnlyPreview
        ? bboxOnlyPreview->attachedLevelForTest()
        : nullptr;
    check(bboxOnlyPreview && bboxOnlyPreviewLevel
            && bboxOnlyPreview->thingViewMode() == GlView::ThingViewMode::Boxes
            && bboxOnlyPreviewLevel->things.size() == 1
            && bboxOnlyPreviewLevel->things.front().bbox.x1 == -1.0
            && bboxOnlyPreviewLevel->things.front().bbox.y1 == -2.0
            && bboxOnlyPreviewLevel->things.front().bbox.z1 == -0.5
            && bboxOnlyPreviewLevel->things.front().bbox.x2 == 2.0
            && bboxOnlyPreviewLevel->things.front().bbox.y2 == 1.0
            && bboxOnlyPreviewLevel->things.front().bbox.z2 == 3.0,
        "Template Editor previews authored BBOX wires when the template has no renderable resource geometry");

    QSettings templateGeometrySettings = settingsStore();
    const QString templateGeometryKey = QStringLiteral("templates/editor_geometry");
    const bool hadTemplateGeometry = templateGeometrySettings.contains(templateGeometryKey);
    const QVariant previousTemplateGeometry = templateGeometrySettings.value(templateGeometryKey);
    QDialog appGeometrySeed{};
    appGeometrySeed.resize(1280, 740);
    appGeometrySeed.move(72, 64);
    const QByteArray appGeometry = appGeometrySeed.saveGeometry();
    templateGeometrySettings.setValue(templateGeometryKey, appGeometry);
    templateGeometrySettings.sync();
    MainWindow appGeometryWindow{};
    std::unique_ptr<QDialog> appGeometryEditor(appGeometryWindow.createTemplateEditorDialogForTest(
        {}, ProjectType::JKDF2, {}, resources));
    check(appGeometryEditor
            && appGeometryEditor->size() == appGeometrySeed.size(),
        "A newly opened Template Editor restores its saved application window size");

    MainWindow geometrySourceWindow{};
    geometrySourceWindow.setAttribute(Qt::WA_DontShowOnScreen);
    geometrySourceWindow.setGeometry(40, 36, 1200, 820);
    geometrySourceWindow.show();
    QCoreApplication::processEvents();
    std::unique_ptr<QDialog> geometrySourceEditor(
        geometrySourceWindow.createTemplateEditorDialogForTest(
            {}, ProjectType::JKDF2, {}, resources));
    if (geometrySourceEditor)
    {
        geometrySourceEditor->setGeometry(110, 96, 1390, 780);
        geometrySourceEditor->show();
        QCoreApplication::processEvents();
    }
    const QByteArray geometrySourceBytes = geometrySourceEditor
        ? geometrySourceEditor->saveGeometry()
        : QByteArray{};
    const QByteArray geometryRecoveryPayload =
        geometrySourceWindow.captureRecoveryEditorStateForTest();
    const QCborMap geometryRoot = QCborValue::fromCbor(geometryRecoveryPayload).toMap();
    const QCborMap geometryWindow = geometryRoot.value(QStringLiteral("window")).toMap();
    const bool geometryCaptured = geometrySourceEditor && geometrySourceEditor->isVisible()
        && !geometrySourceBytes.isEmpty()
        && geometryWindow.value(QStringLiteral("templateEditorGeometry")).toByteArray()
            == geometrySourceBytes;
    check(geometryCaptured,
        "SED workspace captures the visible Template Editor size and position");
    const QSize sourceEditorSize = geometrySourceEditor
        ? geometrySourceEditor->size()
        : QSize{};
    const QPoint sourceEditorPosition = geometrySourceEditor
        ? geometrySourceEditor->pos()
        : QPoint{};
    if (geometrySourceEditor)
    {
        geometrySourceEditor->hide();
    }

    MainWindow geometryTargetWindow{};
    geometryTargetWindow.setAttribute(Qt::WA_DontShowOnScreen);
    geometryTargetWindow.setGeometry(geometrySourceWindow.geometry());
    geometryTargetWindow.show();
    QCoreApplication::processEvents();
    templateGeometrySettings.setValue(templateGeometryKey, appGeometry);
    templateGeometrySettings.sync();
    const bool recoveryAccepted = geometryTargetWindow.restoreRecoveryEditorStateForTest(
        geometryRecoveryPayload);
    templateGeometrySettings.sync();
    const bool geometryRestored = recoveryAccepted
        && templateGeometrySettings.value(templateGeometryKey).toByteArray() == geometrySourceBytes;
    check(geometryRestored,
        "SED workspace geometry replaces the application fallback when recovered");

    std::unique_ptr<QDialog> geometryTargetEditor(
        geometryTargetWindow.createTemplateEditorDialogForTest(
            {}, ProjectType::JKDF2, {}, resources));
    if (geometryTargetEditor)
    {
        geometryTargetEditor->show();
        QCoreApplication::processEvents();
    }
    check(geometryTargetEditor && geometrySourceEditor
            && geometryTargetEditor->size() == sourceEditorSize
            && geometryTargetEditor->pos() == sourceEditorPosition,
        "A fresh Template Editor restores the SED workspace size and visible position");
    if (geometryTargetEditor)
    {
        geometryTargetEditor->hide();
    }
    geometryTargetWindow.hide();
    geometrySourceWindow.hide();

    QCborMap legacyGeometryRoot = geometryRoot;
    QCborMap legacyGeometryWindow = legacyGeometryRoot.value(QStringLiteral("window")).toMap();
    legacyGeometryWindow.remove(QStringLiteral("templateEditorGeometry"));
    legacyGeometryRoot.insert(QStringLiteral("window"), legacyGeometryWindow);
    const QByteArray legacyGeometryPayload = QCborValue(legacyGeometryRoot).toCbor();
    templateGeometrySettings.setValue(templateGeometryKey, appGeometry);
    MainWindow legacyGeometryTarget{};
    const bool legacyGeometryRestored =
        legacyGeometryTarget.restoreRecoveryEditorStateForTest(legacyGeometryPayload)
        && templateGeometrySettings.value(templateGeometryKey).toByteArray() == appGeometry;
    check(legacyGeometryRestored,
        "Older SED workspace state without Template Editor geometry keeps the application fallback");
    if (hadTemplateGeometry)
    {
        templateGeometrySettings.setValue(templateGeometryKey, previousTemplateGeometry);
    }
    else
    {
        templateGeometrySettings.remove(templateGeometryKey);
    }

    ThingTemplate bboxTemplate{};
    bboxTemplate.name = "compact_bbox_fields_template";
    bboxTemplate.parent = "none";
    std::unique_ptr<QDialog> bboxEditor(window.createTemplateEditorDialogForTest(
        { bboxTemplate }, ProjectType::JKDF2, {}, resources));
    check(bboxEditor != nullptr,
        "Template Editor opens for the BBox scrub sizing fixture");
    if (bboxEditor)
    {
        auto* bboxToggle = bboxEditor->findChild<QToolButton*>(
            QStringLiteral("templateEditorBBoxToggle"));
        auto* bboxPanel = bboxEditor->findChild<QWidget*>(
            QStringLiteral("templateEditorBBoxPanel"));
        check(bboxToggle && bboxPanel && !bboxToggle->isChecked() && bboxPanel->isHidden(),
            "Template Editor keeps authored BBOX fields collapsed by default");
        if (bboxToggle && bboxPanel)
        {
            bboxToggle->click();
            check(bboxToggle->isChecked() && !bboxPanel->isHidden(),
                "Template Editor expands the BBOX description before its coordinate fields");
        }
        static const std::array<const char*, 6> kBBoxFields = {
            "templateEditorBBoxX1", "templateEditorBBoxY1", "templateEditorBBoxZ1",
            "templateEditorBBoxX2", "templateEditorBBoxY2", "templateEditorBBoxZ2",
        };
        bool allSteppersCompact = true;
        for (const char* fieldName : kBBoxFields)
        {
            auto* field = bboxEditor->findChild<ui::ScrubFloatEdit*>(QString::fromLatin1(fieldName));
            const auto steppers = field ? field->findChildren<QToolButton*>() : QList<QToolButton*>{};
            allSteppersCompact = allSteppersCompact && field && steppers.size() == 2;
            for (QToolButton* stepper : steppers)
            {
                allSteppersCompact = allSteppersCompact
                    && stepper->minimumWidth() == 12
                    && stepper->maximumWidth() == 12;
            }
        }
        check(allSteppersCompact,
            "All six Template Editor BBox fields keep compact 12-pixel arrows inside their scrub controls");
    }

    const QByteArray material = fixtures::makeIndexedMatFixture(6u);
    const QByteArray colormap = fixtures::makeClassicCmpFixture(6u, QColor(0, 255, 0));
    const QByteArray sprite = QByteArrayLiteral("preview.mat 0 1 2 4 3 1 0 0 0 1 0 0 0\n");
    {
        std::ofstream output(resourceRoot / "preview.mat", std::ios::binary);
        output.write(material.constData(), material.size());
    }
    {
        std::ofstream output(resourceRoot / "preview.cmp", std::ios::binary);
        output.write(colormap.constData(), colormap.size());
    }
    {
        std::ofstream output(resourceRoot / "preview.spr", std::ios::binary);
        output.write(sprite.constData(), sprite.size());
    }
    ThingTemplate spriteTemplate{};
    spriteTemplate.name = "sprite_collision_template";
    spriteTemplate.parent = "none";
    spriteTemplate.values = {
        stringField("sprite", "preview.spr"),
        integerField("collide", 1),
        floatField("size", 1.25),
        floatField("collwidth", 0.75),
        floatField("collheight", 1.5),
    };
    std::unique_ptr<QDialog> spriteEditor(window.createTemplateEditorDialogForTest(
        { spriteTemplate }, ProjectType::IJIM, "preview.cmp", resources));
    auto* spritePreview = spriteEditor
        ? spriteEditor->findChild<GlView*>(QStringLiteral("templatePreviewView"))
        : nullptr;
    auto* spriteStack = spriteEditor
        ? spriteEditor->findChild<QStackedWidget*>(QStringLiteral("templatePreviewStack"))
        : nullptr;
    auto* spriteLabel = spriteEditor
        ? spriteEditor->findChild<QLabel*>(QStringLiteral("templatePreviewSprite"))
        : nullptr;
    auto* spriteCollisionToggle = spriteEditor
        ? spriteEditor->findChild<QToolButton*>(QStringLiteral("templatePreviewCollisionToggle"))
        : nullptr;
    check(spritePreview && spriteStack && spriteLabel && spriteCollisionToggle
            && spriteCollisionToggle->text().isEmpty()
            && !spriteCollisionToggle->isHidden()
            && spriteCollisionToggle->isChecked()
            && spriteStack->currentWidget() == spritePreview,
        "Valid SPR templates default to the collision preview and expose an icon-only toggle");
    if (spriteEditor && spritePreview && spriteStack && spriteLabel && spriteCollisionToggle)
    {
        check(guideBounds(spritePreview).has_value()
                && spritePreview->thingCollisionOverlayExtentForTest(0, true).has_value(),
            "SPR collision preview displays live collision and IJIM guide geometry by default");
        check(editTemplateParameter(*spriteEditor, QStringLiteral("size"), QStringLiteral("2.75")),
            "SPR spatial collision preview allows live editing of effective size");
        const std::optional<double> spriteEditedExtent =
            spritePreview->thingCollisionOverlayExtentForTest(0, true);
        check(spriteEditedExtent && close(*spriteEditedExtent, 2.75, 1.0e-5),
            "SPR spatial collision preview updates the collide sphere after size edits");
        spriteCollisionToggle->click();
        auto* spriteLegend = spriteEditor->findChild<QLabel*>(QStringLiteral("templatePreviewCollisionLegend"));
        check(!spriteCollisionToggle->isChecked()
                && spriteStack->currentWidget() == spriteLabel
                && spriteLegend && spriteLegend->isHidden(),
            "Turning collision preview off returns to the SPR thumbnail and hides its description");
        spriteCollisionToggle->click();
        check(spriteCollisionToggle->isChecked()
                && spriteStack->currentWidget() == spritePreview
                && spriteLegend && !spriteLegend->isHidden(),
            "Turning collision preview on restores the SPR spatial collision view");
    }
}

} // namespace sed::app_tests
