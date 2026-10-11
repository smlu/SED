#include "main_window/common.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include "ui/components/live_color_popup.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"
#include "ui/lighting/mots_arch_lighting_dialog.hpp"
#include "ui/layers/layer_color_utils.hpp"
#include "thing/thing_resource_utils.hpp"
#include "sed/level/arch_lighting_clipboard.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMargins>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScreen>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace sed
{

namespace
{

constexpr auto kMotsArchLightingClipboardMimeType =
    "application/x-sed-mots-archlighting-v1";

bool finiteVertex(const MotsArchLightVertex& vertex) noexcept
{
    return std::isfinite(vertex.mono)
        && std::isfinite(vertex.red)
        && std::isfinite(vertex.green)
        && std::isfinite(vertex.blue);
}

bool sameVertexBits(
    const MotsArchLightVertex& left,
    const MotsArchLightVertex& right) noexcept
{
    return std::bit_cast<std::uint32_t>(left.mono)
            == std::bit_cast<std::uint32_t>(right.mono)
        && std::bit_cast<std::uint32_t>(left.red)
            == std::bit_cast<std::uint32_t>(right.red)
        && std::bit_cast<std::uint32_t>(left.green)
            == std::bit_cast<std::uint32_t>(right.green)
        && std::bit_cast<std::uint32_t>(left.blue)
            == std::bit_cast<std::uint32_t>(right.blue);
}

bool sameTopology(
    const level::MotsArchLightingTopology& left,
    const level::MotsArchLightingTopology& right) noexcept
{
    return left.geosets == right.geosets;
}

const ui::lighting::MotsArchLightingTarget* findTarget(
    const std::vector<ui::lighting::MotsArchLightingTarget>& targets,
    const std::int32_t thingIndex) noexcept
{
    const auto it = std::find_if(
        targets.begin(), targets.end(), [thingIndex](const auto& target)
        {
            return target.thingIndex == thingIndex;
        });
    return it == targets.end() ? nullptr : &*it;
}

std::optional<MotsArchLightVertex> authoredOrDefaultVertex(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::int32_t expectedRecordIndex,
    const std::int32_t meshIndex,
    const std::int32_t vertexIndex) noexcept
{
    if (thing.archLightingIndex != expectedRecordIndex
        || !level::isMotsArchThing(level, thing, templates))
    {
        return std::nullopt;
    }
    if (expectedRecordIndex == -1)
    {
        return MotsArchLightVertex{ 0.0f, 0.0f, 0.0f, 0.0f };
    }
    if (expectedRecordIndex < 0
        || static_cast<std::size_t>(expectedRecordIndex)
            >= level.motsArchLighting.size()
        || meshIndex < 0 || vertexIndex < 0)
    {
        return std::nullopt;
    }
    const MotsArchLightObject& object = level.motsArchLighting[
        static_cast<std::size_t>(expectedRecordIndex)];
    const std::size_t mesh = static_cast<std::size_t>(meshIndex);
    const std::size_t vertex = static_cast<std::size_t>(vertexIndex);
    if (mesh >= object.meshes.size()
        || vertex >= object.meshes[mesh].vertices.size())
    {
        return std::nullopt;
    }
    const MotsArchLightVertex value = object.meshes[mesh].vertices[vertex];
    return finiteVertex(value)
        ? std::optional<MotsArchLightVertex>{ value }
        : std::nullopt;
}

bool validVertexInTopology(
    const level::MotsArchLightingTopology& topology,
    const std::int32_t meshIndex,
    const std::int32_t vertexIndex) noexcept
{
    if (meshIndex < 0 || vertexIndex < 0)
    {
        return false;
    }
    const std::size_t mesh = static_cast<std::size_t>(meshIndex);
    const std::size_t vertex = static_cast<std::size_t>(vertexIndex);
    for (const auto& geoset : topology.geosets)
    {
        if (mesh < geoset.size() && vertex < geoset[mesh])
        {
            return true;
        }
    }
    return false;
}

void sizeInteractiveMotsArchLightingScrub(ui::ScrubFloatEdit* const scrub)
{
    if (!scrub || !scrub->lineEdit())
    {
        return;
    }

    scrub->ensurePolished();
    QLineEdit* const lineEdit = scrub->lineEdit();
    lineEdit->ensurePolished();
    const QFontMetrics metrics(lineEdit->font());
    // Keep the authored float readable when the scrub arrows are revealed.
    // The control accepts unrestricted finite float payloads, so this is a
    // signed nine-decimal HDR baseline rather than an input range.
    const int valueWidth = metrics.horizontalAdvance(
        QStringLiteral("-1000000.000000000"));
    const QMargins margins = lineEdit->textMargins();
    const int frameWidth = lineEdit->style()->pixelMetric(
        QStyle::PM_DefaultFrameWidth, nullptr, lineEdit);

    int totalStepperWidth = 0;
    const QList<QToolButton*> steppers = scrub->findChildren<QToolButton*>();
    for (QToolButton* const stepper : steppers)
    {
        if (!stepper)
        {
            continue;
        }
        stepper->ensurePolished();
        const QFontMetrics stepperMetrics(stepper->font());
        const int styleMargin = stepper->style()->pixelMetric(
            QStyle::PM_ButtonMargin, nullptr, stepper);
        const int horizontalPadding = std::max({
            2,
            stepperMetrics.height() / 8,
            styleMargin / 2 });
        const int frame = stepper->style()->pixelMetric(
            QStyle::PM_DefaultFrameWidth, nullptr, stepper);
        const int compactWidth = stepperMetrics.horizontalAdvance(
            stepper->text()) + 2 * horizontalPadding + 2 * frame;
        stepper->setMinimumWidth(0);
        stepper->setFixedWidth(std::max(1, compactWidth));
        totalStepperWidth += std::max(1, compactWidth);
    }

    const int textWidth = valueWidth + metrics.averageCharWidth();
    const int lineEditWidth = textWidth
        + margins.left() + margins.right() + 2 * frameWidth + 2;
    lineEdit->setMinimumWidth(lineEditWidth);
    scrub->setMinimumWidth(lineEditWidth + totalStepperWidth);
}

std::optional<level::MotsArchLightingClipboardPayload>
readMotsArchLightingClipboard(QString& error)
{
    error.clear();
    const QClipboard* const clipboard = QApplication::clipboard();
    const QMimeData* const mime = clipboard ? clipboard->mimeData() : nullptr;
    if (!mime || !mime->hasFormat(
            QString::fromLatin1(kMotsArchLightingClipboardMimeType)))
    {
        error = QObject::tr("The system clipboard does not contain ARCHLIGHTING.");
        return std::nullopt;
    }
    const QByteArray encoded = mime->data(
        QString::fromLatin1(kMotsArchLightingClipboardMimeType));
    const std::span<const std::byte> bytes(
        reinterpret_cast<const std::byte*>(encoded.constData()),
        static_cast<std::size_t>(encoded.size()));
    std::string decodeError{};
    const auto payload = level::decodeMotsArchLightingClipboard(
        bytes, &decodeError);
    if (!payload)
    {
        error = decodeError.empty()
            ? QObject::tr("The ARCHLIGHTING clipboard payload is invalid.")
            : QString::fromStdString(decodeError);
    }
    return payload;
}

void writeMotsArchLightingClipboard(
    const level::MotsArchLightingClipboardPayload& payload,
    const std::vector<std::byte>& encoded)
{
    if (QClipboard* const clipboard = QApplication::clipboard())
    {
        auto* const mime = new QMimeData();
        mime->setData(
            QString::fromLatin1(kMotsArchLightingClipboardMimeType),
            QByteArray(
                reinterpret_cast<const char*>(encoded.data()),
                static_cast<qsizetype>(encoded.size())));
        mime->setText(QStringLiteral("SED: ARCHLIGHTING %1")
            .arg(QString::fromStdString(payload.modelIdentity)));
        clipboard->setMimeData(mime);
    }
}

} // namespace

void MainWindow::clearArchitecturalLightingInteractiveState() noexcept
{
    if (architecturalLightingColorDialog_)
    {
        QColorDialog* const picker = architecturalLightingColorDialog_.data();
        architecturalLightingColorDialog_.clear();
        if (picker)
        {
            picker->close();
            picker->deleteLater();
        }
    }
    if (architecturalLightingVertexDialog_)
    {
        QPointer<QDialog> dialog = architecturalLightingVertexDialog_;
        architecturalLightingVertexDialog_.clear();
        if (dialog)
        {
            dialog->blockSignals(true);
            dialog->close();
            dialog->deleteLater();
        }
    }
    architecturalLightingInteractive_.clear();
    if (glView_)
    {
        glView_->clearArchitecturalLightingPreview();
        glView_->update();
    }
}

bool MainWindow::refreshArchitecturalLightingInteractiveOwner(
    const std::int32_t thingIndex)
{
    clearArchitecturalLightingInteractiveState();
    if (editMode_ != EditMode::light
        || lightEditSubMode_ != LightEditSubMode::architectural
        || document_.level().kind != ProjectType::MOTS
        || thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= document_.level().things.size())
    {
        return false;
    }

    const assets::Templates* templatesDb = templates_.size() > 0
        ? &templates_
        : nullptr;
    const auto target = ui::lighting::collectMotsArchLightingTarget(
        document_.level(), templatesDb, &resources_, thingIndex);
    if (!target || !target->modelAvailable || !target->valueEditingAllowed)
    {
        return false;
    }

    const Thing& thing = document_.level().things[
        static_cast<std::size_t>(thingIndex)];
    if (isLayerLocked(thing.layer))
    {
        return false;
    }

    ArchitecturalLightingInteractiveState& state =
        architecturalLightingInteractive_;
    state.ownerThingIndex = thingIndex;
    state.sourceRecordIndex = thing.archLightingIndex;
    state.documentEpoch = recoveryDocumentEpoch_;
    state.undoSignature = document_.undoSignature();
    state.resourceIdentity = resources_.previewCacheIdentity();
    state.modelIdentity = target->resourceRevision;
    state.topology = target->topology;
    state.active = true;

    if (thing.archLightingIndex >= 0
        && static_cast<std::size_t>(thing.archLightingIndex)
            < document_.level().motsArchLighting.size())
    {
        state.baseline = document_.level().motsArchLighting[
            static_cast<std::size_t>(thing.archLightingIndex)];
    }
    return true;
}

bool MainWindow::prepareArchitecturalLightingInteractiveSelection(
    const std::vector<ArchitecturalLightingVertexSelection>& vertices)
{
    clearArchitecturalLightingInteractiveState();
    if (vertices.empty()
        || editMode_ != EditMode::light
        || lightEditSubMode_ != LightEditSubMode::architectural
        || document_.level().kind != ProjectType::MOTS
        || vertices.size() > 4096u)
    {
        return false;
    }

    const Level& level = document_.level();
    const assets::Templates* templatesDb = templates_.size() > 0
        ? &templates_
        : nullptr;
    std::vector<std::int32_t> owners{};
    owners.reserve(vertices.size());
    for (const ArchitecturalLightingVertexSelection& vertex : vertices)
    {
        if (!vertex.valid()
            || static_cast<std::size_t>(vertex.thingIndex) >= level.things.size()
            || isLayerLocked(level.things[static_cast<std::size_t>(vertex.thingIndex)].layer)
            || !ui::layers::isLayerVisible(layerVisible_,
                level.things[static_cast<std::size_t>(vertex.thingIndex)].layer))
        {
            clearArchitecturalLightingInteractiveState();
            return false;
        }
        if (std::find(owners.begin(), owners.end(), vertex.thingIndex)
            == owners.end())
        {
            owners.push_back(vertex.thingIndex);
        }
    }

    const std::vector<ui::lighting::MotsArchLightingTarget> targets =
        ui::lighting::collectMotsArchLightingTargetsForThings(
            level, templatesDb, &resources_, owners);
    if (targets.size() != owners.size())
    {
        clearArchitecturalLightingInteractiveState();
        return false;
    }

    const ui::lighting::MotsArchLightingTarget* firstTarget =
        findTarget(targets, owners.front());
    if (!firstTarget || !firstTarget->modelAvailable
        || !firstTarget->valueEditingAllowed
        || firstTarget->resourceRevision.empty()
        || firstTarget->originalRecordIndex < -1
        || (firstTarget->originalRecordIndex >= 0
            && static_cast<std::size_t>(firstTarget->originalRecordIndex)
                >= level.motsArchLighting.size()))
    {
        clearArchitecturalLightingInteractiveState();
        return false;
    }

    std::vector<ArchitecturalLightingVertexSelection> sortedVertices = vertices;
    std::sort(
        sortedVertices.begin(), sortedVertices.end(),
        [](const ArchitecturalLightingVertexSelection& left,
           const ArchitecturalLightingVertexSelection& right)
        {
            if (left.thingIndex != right.thingIndex)
            {
                return left.thingIndex < right.thingIndex;
            }
            if (left.lodIndex != right.lodIndex)
            {
                return left.lodIndex < right.lodIndex;
            }
            if (left.meshIndex != right.meshIndex)
            {
                return left.meshIndex < right.meshIndex;
            }
            return left.vertexIndex < right.vertexIndex;
        });
    if (std::adjacent_find(sortedVertices.begin(), sortedVertices.end())
        != sortedVertices.end())
    {
        clearArchitecturalLightingInteractiveState();
        return false;
    }

    ArchitecturalLightingInteractiveState& state =
        architecturalLightingInteractive_;
    state.ownerThingIndex = owners.front();
    state.sourceRecordIndex = firstTarget->originalRecordIndex;
    state.documentEpoch = recoveryDocumentEpoch_;
    state.undoSignature = document_.undoSignature();
    state.resourceIdentity = resources_.previewCacheIdentity();
    state.modelIdentity = firstTarget->resourceRevision;
    state.topology = firstTarget->topology;
    if (firstTarget->originalRecordIndex >= 0)
    {
        state.baseline = level.motsArchLighting[
            static_cast<std::size_t>(firstTarget->originalRecordIndex)];
    }
    state.active = true;

    state.selectedVertices.reserve(vertices.size());
    for (const ArchitecturalLightingVertexSelection& vertex : vertices)
    {
        const Thing& thing = level.things[static_cast<std::size_t>(vertex.thingIndex)];
        const auto* ownerTarget = findTarget(targets, vertex.thingIndex);
        if (thing.archLightingIndex < -1
            || !ownerTarget
            || !ownerTarget->modelAvailable
            || !ownerTarget->valueEditingAllowed
            || ownerTarget->resourceRevision.empty()
            || ownerTarget->originalRecordIndex != thing.archLightingIndex
            || !validVertexInTopology(
                ownerTarget->topology, vertex.meshIndex, vertex.vertexIndex))
        {
            clearArchitecturalLightingInteractiveState();
            return false;
        }
        const std::optional<MotsArchLightVertex> authoredValue =
            authoredOrDefaultVertex(
                level, templatesDb, thing, thing.archLightingIndex,
                vertex.meshIndex, vertex.vertexIndex);
        if (!authoredValue)
        {
            clearArchitecturalLightingInteractiveState();
            return false;
        }
        const MotsArchLightVertex authored = *authoredValue;
        state.selectedVertices.push_back(
            ArchitecturalLightingInteractiveState::SelectedVertex{
                vertex,
                thing.archLightingIndex,
                ownerTarget->resourceRevision,
                ownerTarget->topology,
                authored,
                authored,
                { false, false, false, false } });
    }
    if (state.selectedVertices.empty())
    {
        clearArchitecturalLightingInteractiveState();
        return false;
    }
    state.vertex = state.selectedVertices.front().selection;
    state.baselineVertex = state.selectedVertices.front().baseline;
    state.previewVertex = state.selectedVertices.front().preview;
    return true;
}

void MainWindow::openArchitecturalLightingVertexEditor(
    const std::vector<ArchitecturalLightingVertexSelection>& vertices)
{
    if (vertices.empty()
        || editMode_ != EditMode::light
        || lightEditSubMode_ != LightEditSubMode::architectural)
    {
        return;
    }
    if (architecturalLightingVertexDialog_)
    {
        architecturalLightingVertexDialog_->raise();
        architecturalLightingVertexDialog_->activateWindow();
        return;
    }
    if (!prepareArchitecturalLightingInteractiveSelection(vertices))
    {
        return;
    }
    ArchitecturalLightingInteractiveState& state =
        architecturalLightingInteractive_;
    if (state.selectedVertices.empty() || !glView_)
    {
        return;
    }

    const MotsArchLightVertex initial = state.selectedVertices.front().baseline;
    if (!stageArchitecturalLightingSelectedVertices(
            initial,
            level::MotsArchLightingVertexMode::Rgb,
            std::array<bool, 4>{ false, false, false, false }))
    {
        return;
    }

    auto* const dialog = new QDialog(this);
    architecturalLightingVertexDialog_ = dialog;
    dialog->setObjectName(QStringLiteral("lightParametersPopup"));
    dialog->setWindowTitle(QStringLiteral("Architectural vertex lighting"));
    dialog->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setStyleSheet(QStringLiteral(
        "QDialog#lightParametersPopup { background-color: #252525; border: 1px solid #51565d; border-radius: 6px; }"
        "QLabel#motsArchLightingInteractiveStatus { color: #cfd5dc; }"
        "QLabel#motsArchLightingVertexIdentity { color: #e6e9ed; font-weight: 600; }"));

    auto* const root = new QVBoxLayout(dialog);
    root->setContentsMargins(9, 9, 9, 9);
    root->setSpacing(7);
    const auto firstSelection = state.selectedVertices.front().selection;
    auto* const identity = new QLabel(
        state.selectedVertices.size() == 1
            ? tr("Thing %1, LOD %2, mesh %3, vertex %4")
                .arg(firstSelection.thingIndex)
                .arg(firstSelection.lodIndex)
                .arg(firstSelection.meshIndex)
                .arg(firstSelection.vertexIndex)
            : tr("%1 selected vertices across %2 Things")
                .arg(state.selectedVertices.size())
                .arg([&state]()
                {
                    std::unordered_set<std::int32_t> owners{};
                    for (const auto& selected : state.selectedVertices)
                    {
                        owners.insert(selected.selection.thingIndex);
                    }
                    return owners.size();
                }()),
        dialog);
    identity->setObjectName(QStringLiteral("motsArchLightingVertexIdentity"));
    root->addWidget(identity);

    auto* const form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(5);
    auto* const mode = new QComboBox(dialog);
    mode->setObjectName(QStringLiteral("motsArchLightingInteractiveMode"));
    mode->addItem(QStringLiteral("RGB color"),
        static_cast<int>(level::MotsArchLightingVertexMode::Rgb));
    mode->addItem(QStringLiteral("Mono grayscale"),
        static_cast<int>(level::MotsArchLightingVertexMode::Mono));
    if (glView_)
    {
        const int currentMode = mode->findData(static_cast<int>(
            glView_->architecturalLightingAuthoringMode()));
        if (currentMode >= 0)
        {
            mode->setCurrentIndex(currentMode);
        }
    }
    form->addRow(tr("Edit channel"), mode);

    const auto makeChannel = [dialog](const char* objectName, const float value)
    {
        auto* const edit = new ui::ScrubFloatEdit(dialog);
        edit->setObjectName(QString::fromLatin1(objectName));
        edit->clearRange();
        edit->setDecimals(9);
        edit->setSingleStep(0.01);
        edit->setStepperUsesScrubSignals(true);
        edit->setValue(static_cast<double>(value));
        sizeInteractiveMotsArchLightingScrub(edit);
        return edit;
    };
    auto* const mono = makeChannel(
        "motsArchLightingInteractiveMono", initial.mono);
    auto* const monoSlider = new QSlider(Qt::Horizontal, dialog);
    monoSlider->setObjectName(QStringLiteral("motsArchLightingInteractiveMonoSlider"));
    monoSlider->setRange(0, 1000);
    monoSlider->setTracking(true);
    monoSlider->setToolTip(
        tr("Sets the stored Mono channel and previews it as grayscale in the ARCH editor. Type a raw value for signed or HDR data."));
    monoSlider->setValue(static_cast<int>(std::lround(
        std::clamp(static_cast<double>(initial.mono), 0.0, 1.0) * 1000.0)));
    const auto finitePositive = [](const float value)
    {
        return std::isfinite(value) && value > 0.0f
            ? static_cast<double>(value)
            : 0.0;
    };
    const double initialRgbIntensity = std::max({
        finitePositive(initial.red),
        finitePositive(initial.green),
        finitePositive(initial.blue),
        0.0 });
    const double displayRgbScale = initialRgbIntensity > 1.0e-12
        ? initialRgbIntensity
        : 1.0;
    auto* const rgbIntensity = makeChannel(
        "motsArchLightingInteractiveRgbIntensity",
        static_cast<float>(initialRgbIntensity));
    rgbIntensity->setToolTip(
        tr("RGB intensity multiplier; changing it scales each selected vertex's authored RGB."));
    auto* const monoLabel = new QLabel(tr("Mono value"), dialog);
    auto* const monoSliderLabel = new QLabel(tr("Mono grayscale"), dialog);
    auto* const rgbLabel = new QLabel(tr("RGB intensity"), dialog);
    form->addRow(monoLabel, mono);
    form->addRow(monoSliderLabel, monoSlider);
    form->addRow(rgbLabel, rgbIntensity);
    root->addLayout(form);

    const auto initialDisplayColor = [&]()
    {
        const bool black = initial.red == 0.0f
            && initial.green == 0.0f
            && initial.blue == 0.0f;
        if (black)
        {
            return QColor::fromRgbF(1.0, 1.0, 1.0);
        }
        const double red = finitePositive(initial.red) / displayRgbScale;
        const double green = finitePositive(initial.green) / displayRgbScale;
        const double blue = finitePositive(initial.blue) / displayRgbScale;
        return QColor::fromRgbF(
            std::clamp(red, 0.0, 1.0),
            std::clamp(green, 0.0, 1.0),
            std::clamp(blue, 0.0, 1.0));
    }();
    const std::shared_ptr<std::array<bool, 4>> dirtyChannels =
        std::make_shared<std::array<bool, 4>>(std::array<bool, 4>{});
    const std::shared_ptr<bool> rgbIntensityDirty =
        std::make_shared<bool>(false);
    const std::shared_ptr<bool> rgbColorDirty =
        std::make_shared<bool>(false);
    const std::shared_ptr<bool> inputValid = std::make_shared<bool>(true);
    const std::shared_ptr<std::function<void()>> stageFromControls =
        std::make_shared<std::function<void()>>();
    const auto rgbDisplayColor = std::make_shared<QColor>(initialDisplayColor);
    const auto monoDisplayColor = std::make_shared<QColor>([&]()
    {
        const float gray = static_cast<float>(
            std::clamp(initial.mono, 0.0f, 1.0f));
        return QColor::fromRgbF(gray, gray, gray);
    }());
    const auto colorEditorHolder =
        std::make_shared<QPointer<ui::LiveColorPopup>>();

    auto* const colorEditor = new ui::LiveColorPopup(
        dialog,
        initialDisplayColor,
        false,
        [stageFromControls, dirtyChannels, rgbColorDirty, rgbIntensityDirty,
            rgbIntensity, mode, mono, monoSlider, rgbDisplayColor,
            monoDisplayColor, colorEditorHolder](
            const QColor& color)
        {
            if (mode->currentData().toInt()
                == static_cast<int>(level::MotsArchLightingVertexMode::Mono))
            {
                const float gray = static_cast<float>(
                    std::clamp(0.2126 * color.redF()
                        + 0.7152 * color.greenF()
                        + 0.0722 * color.blueF(), 0.0, 1.0));
                *monoDisplayColor = QColor::fromRgbF(gray, gray, gray);
                mono->setValue(gray);
                {
                    const QSignalBlocker blocker(monoSlider);
                    monoSlider->setValue(static_cast<int>(std::lround(gray * 1000.0f)));
                }
                if (*colorEditorHolder)
                {
                    (*colorEditorHolder)->setCurrentColor(
                        *monoDisplayColor, false);
                }
                (*dirtyChannels)[0] = true;
            }
            else
            {
                *rgbDisplayColor = color;
                if (rgbIntensity->value() <= 1.0e-12
                    && (color.red() != 0
                        || color.green() != 0
                        || color.blue() != 0))
                {
                    rgbIntensity->setValue(1.0);
                    *rgbIntensityDirty = true;
                }
                (*rgbColorDirty) = true;
                (*dirtyChannels)[1] = true;
                (*dirtyChannels)[2] = true;
                (*dirtyChannels)[3] = true;
            }
            if (*stageFromControls)
            {
                (*stageFromControls)();
            }
        },
        {},
        ui::LiveColorPopup::Presentation::Embedded);
    *colorEditorHolder = colorEditor;
    colorEditor->setObjectName(QStringLiteral("motsArchLightingInteractiveColor"));
    root->addWidget(colorEditor);

    auto* const status = new QLabel(dialog);
    status->setObjectName(QStringLiteral("motsArchLightingInteractiveStatus"));
    status->setWordWrap(true);
    root->addWidget(status);

    const QPointer<QDialog> editorGuard(dialog);
    const QPointer<ui::ScrubFloatEdit> monoGuard(mono);
    const QPointer<ui::ScrubFloatEdit> rgbIntensityGuard(rgbIntensity);
    const QPointer<ui::LiveColorPopup> colorGuard(colorEditor);
    const QPointer<QComboBox> modeGuard(mode);
    const QPointer<QLabel> statusGuard(status);
    std::array<bool, 4> mixedValues{};
    for (std::size_t channel = 0; channel < mixedValues.size(); ++channel)
    {
        for (const auto& selected : state.selectedVertices)
        {
            const float firstValue = channel == 0
                ? initial.mono
                : channel == 1 ? initial.red
                : channel == 2 ? initial.green
                : initial.blue;
            const float candidate = channel == 0
                ? selected.baseline.mono
                : channel == 1 ? selected.baseline.red
                : channel == 2 ? selected.baseline.green
                : selected.baseline.blue;
            if (std::bit_cast<std::uint32_t>(firstValue)
                != std::bit_cast<std::uint32_t>(candidate))
            {
                mixedValues[channel] = true;
                break;
            }
        }
    }
    const bool anyMixed = std::any_of(
        mixedValues.begin(), mixedValues.end(), [](const bool mixed)
        {
            return mixed;
        });

    *stageFromControls = [this, dirtyChannels, rgbIntensityDirty,
        rgbColorDirty, editorGuard, modeGuard, monoGuard, rgbIntensityGuard,
        colorGuard, statusGuard, inputValid, mixedValues,
        anyMixed, initialRgbIntensity]()
    {
        if (!editorGuard
            || architecturalLightingVertexDialog_.data() != editorGuard.data()
            || !modeGuard || !monoGuard || !rgbIntensityGuard || !colorGuard)
        {
            return;
        }
        const auto parseFiniteFloat = [](const ui::ScrubFloatEdit* const edit)
            -> std::optional<float>
        {
            if (!edit || !edit->lineEdit())
            {
                return std::nullopt;
            }
            bool parsed = false;
            const double value = ui::parseUiNumber(edit->lineEdit()->text(),
                edit->lineEdit()->locale(), &parsed);
            const float narrowed = static_cast<float>(value);
            if (!parsed || !std::isfinite(value) || !std::isfinite(narrowed))
            {
                return std::nullopt;
            }
            return narrowed;
        };
        const std::optional<float> monoParsed = parseFiniteFloat(monoGuard);
        const std::optional<float> intensityParsed =
            parseFiniteFloat(rgbIntensityGuard);
        if (((*dirtyChannels)[0] && !monoParsed)
            || (*rgbIntensityDirty && !intensityParsed))
        {
            *inputValid = false;
            if (statusGuard)
            {
                statusGuard->setText(QObject::tr(
                    "Enter a finite float for every edited intensity."));
            }
            return;
        }

        const bool monoMode = modeGuard->currentData().toInt()
            == static_cast<int>(level::MotsArchLightingVertexMode::Mono);
        const float monoValue = monoParsed.value_or(
            static_cast<float>(monoGuard->value()));
        const float intensity = intensityParsed.value_or(
            static_cast<float>(rgbIntensityGuard->value()));
        const QColor color = colorGuard->currentColor();
        std::vector<MotsArchLightVertex> values{};
        values.reserve(architecturalLightingInteractive_.selectedVertices.size());
        for (const auto& selected :
             architecturalLightingInteractive_.selectedVertices)
        {
            MotsArchLightVertex value = selected.preview;
            if ((*dirtyChannels)[0])
            {
                value.mono = monoValue;
            }
            if (!monoMode && (*rgbIntensityDirty || *rgbColorDirty))
            {
                if (*rgbColorDirty)
                {
                    value.red = static_cast<float>(color.redF() * intensity);
                    value.green = static_cast<float>(color.greenF() * intensity);
                    value.blue = static_cast<float>(color.blueF() * intensity);
                }
                else
                {
                    const float multiplier = initialRgbIntensity > 1.0e-12
                        ? intensity / static_cast<float>(initialRgbIntensity)
                        : intensity;
                    const bool baselineIsBlack = selected.baseline.red == 0.0f
                        && selected.baseline.green == 0.0f
                        && selected.baseline.blue == 0.0f;
                    if (baselineIsBlack)
                    {
                        // Black has no authored chromaticity.  An explicit
                        // intensity edit gives it a neutral white basis so
                        // the control has a useful, deterministic result.
                        value.red = intensity;
                        value.green = intensity;
                        value.blue = intensity;
                    }
                    else
                    {
                        value.red = selected.baseline.red * multiplier;
                        value.green = selected.baseline.green * multiplier;
                        value.blue = selected.baseline.blue * multiplier;
                    }
                }
            }
            values.push_back(value);
        }
        const auto selectedMode = static_cast<level::MotsArchLightingVertexMode>(
            modeGuard->currentData().toInt());
        std::array<bool, 4> effectiveDirty = *dirtyChannels;
        if (!monoMode && (*rgbIntensityDirty || *rgbColorDirty))
        {
            effectiveDirty[1] = true;
            effectiveDirty[2] = true;
            effectiveDirty[3] = true;
        }
        if (!stageArchitecturalLightingSelectedVertices(
                values, selectedMode, effectiveDirty))
        {
            *inputValid = false;
            if (statusGuard)
            {
                statusGuard->setText(QObject::tr(
                    "The selected owner or model changed. Choose the vertex again."));
            }
            return;
        }
        *inputValid = true;
        if (!statusGuard)
        {
            return;
        }
        if (anyMixed && std::none_of(
                effectiveDirty.begin(), effectiveDirty.end(),
                [](const bool dirty) { return dirty; }))
        {
            statusGuard->setText(QObject::tr(
                "Mixed values across selected vertices; edit a channel to apply it to all selected vertices."));
        }
        else
        {
            statusGuard->setText(selectedMode
                    == level::MotsArchLightingVertexMode::Mono
                ? QObject::tr("Mono value is staged; closing this popup commits one undo step.")
                : QObject::tr("RGB color and intensity are staged; closing this popup commits one undo step."));
        }
    };

    const auto updateEnabled = [dialog, root, mode, mono, monoSlider, monoLabel,
        monoSliderLabel, rgbIntensity, rgbLabel, colorEditor,
        rgbDisplayColor, monoDisplayColor]()
    {
        const bool monoMode = mode->currentData().toInt()
            == static_cast<int>(level::MotsArchLightingVertexMode::Mono);
        mono->setEnabled(monoMode);
        monoSlider->setEnabled(monoMode);
        rgbIntensity->setEnabled(!monoMode);
        mono->setVisible(monoMode);
        monoLabel->setVisible(monoMode);
        monoSlider->setVisible(monoMode);
        monoSliderLabel->setVisible(monoMode);
        rgbIntensity->setVisible(!monoMode);
        rgbLabel->setVisible(!monoMode);
        if (colorEditor)
        {
            colorEditor->setVisible(!monoMode);
            colorEditor->setCurrentColor(
                monoMode ? *monoDisplayColor : *rgbDisplayColor, false);
        }
        root->invalidate();
        root->activate();
        dialog->adjustSize();
    };
    connect(mode, qOverload<int>(&QComboBox::currentIndexChanged), dialog,
        [this, mode, stageFromControls, updateEnabled]()
        {
            updateEnabled();
            if (glView_)
            {
                glView_->setArchitecturalLightingAuthoringMode(
                    static_cast<level::MotsArchLightingVertexMode>(
                        mode->currentData().toInt()));
            }
            if (*stageFromControls)
            {
                (*stageFromControls)();
            }
        });
    const auto syncMonoDisplayColor = [monoDisplayColor, monoSlider, colorEditor](
        const double value)
    {
        const float gray = static_cast<float>(std::clamp(value, 0.0, 1.0));
        *monoDisplayColor = QColor::fromRgbF(gray, gray, gray);
        {
            const QSignalBlocker blocker(monoSlider);
            monoSlider->setValue(static_cast<int>(std::lround(gray * 1000.0f)));
        }
        colorEditor->setCurrentColor(*monoDisplayColor, false);
    };
    connect(monoSlider, &QSlider::valueChanged, dialog,
        [mono, dirtyChannels, stageFromControls, syncMonoDisplayColor](
            const int value)
        {
            const double monoValue = static_cast<double>(value) / 1000.0;
            mono->setValue(monoValue);
            (*dirtyChannels)[0] = true;
            syncMonoDisplayColor(monoValue);
            (*stageFromControls)();
        });
    connect(mono, &ui::ScrubFloatEdit::scrubValueChanged, dialog,
        [dirtyChannels, stageFromControls](const double)
        {
            (*dirtyChannels)[0] = true;
            (*stageFromControls)();
        });
    connect(mono, &ui::ScrubFloatEdit::scrubValueChanged, dialog,
        syncMonoDisplayColor);
    connect(mono->lineEdit(), &QLineEdit::textEdited, dialog,
        [dirtyChannels, stageFromControls](const QString&)
        {
            (*dirtyChannels)[0] = true;
            (*stageFromControls)();
        });
    connect(mono->lineEdit(), &QLineEdit::textEdited, dialog,
        [mono, syncMonoDisplayColor](const QString& text)
        {
            bool ok = false;
            const double value = ui::parseUiNumber(text, mono->lineEdit()->locale(), &ok);
            if (ok && std::isfinite(value))
            {
                syncMonoDisplayColor(value);
            }
        });
    const auto markRgbIntensity = [rgbIntensityDirty, stageFromControls](
        const double)
    {
        *rgbIntensityDirty = true;
        (*stageFromControls)();
    };
    connect(rgbIntensity, &ui::ScrubFloatEdit::scrubValueChanged, dialog,
        markRgbIntensity);
    connect(rgbIntensity->lineEdit(), &QLineEdit::textEdited, dialog,
        [markRgbIntensity](const QString&) { markRgbIntensity(0.0); });
    connect(dialog, &QDialog::finished, this,
        [this, dialogGuard = QPointer<QDialog>(dialog), inputValid](const int)
        {
            if (!dialogGuard)
            {
                return;
            }
            if (architecturalLightingVertexDialog_.data() == dialogGuard.data())
            {
                // The popup has already begun dismissing. Detach it before
                // committing so clearArchitecturalLightingInteractiveState()
                // does not call close() reentrantly from this finished
                // callback. Explicit cancellation still clears the member
                // first, blocks signals, and closes the popup without
                // committing staged values.
                architecturalLightingVertexDialog_.clear();
                // A buttonless Qt::Popup commits on any dismissal (outside
                // click, Escape, or close). Keep all edits in this popup as one
                // undoable transaction; malformed text abandons the complete
                // staged transaction instead of committing an earlier value.
                if (!*inputValid
                    || !commitArchitecturalLightingInteractiveEdit())
                {
                    cancelArchitecturalLightingInteractiveEdit();
                }
            }
            if (dialogGuard)
            {
                dialogGuard->deleteLater();
            }
        });

    updateEnabled();
    if (anyMixed)
    {
        status->setText(QObject::tr(
            "Mixed values across selected vertices; edit a channel to apply it to all selected vertices."));
    }
    else
    {
        status->setText(QObject::tr(
            "%1 selected vertex%2; edit RGB color/intensity or the stored Mono channel. Closing this popup commits one undo step.")
                .arg(state.selectedVertices.size())
                .arg(state.selectedVertices.size() == 1 ? "" : "es"));
    }
    dialog->adjustSize();
    QPoint globalPosition = QCursor::pos() + QPoint(8, 8);
    if (const QScreen* const screen = QGuiApplication::screenAt(globalPosition))
    {
        const QRect available = screen->availableGeometry().adjusted(6, 6, -6, -6);
        if (globalPosition.x() + dialog->width() > available.right())
        {
            globalPosition.setX(available.right() - dialog->width());
        }
        if (globalPosition.y() + dialog->height() > available.bottom())
        {
            globalPosition.setY(globalPosition.y() - dialog->height() - 16);
        }
        globalPosition.setX(std::max(globalPosition.x(), available.left()));
        globalPosition.setY(std::max(globalPosition.y(), available.top()));
    }
    dialog->move(globalPosition);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::openArchitecturalLightingVertexEditor(
    const ArchitecturalLightingVertexSelection& vertex)
{
    openArchitecturalLightingVertexEditor(
        std::vector<ArchitecturalLightingVertexSelection>{ vertex });
}

bool MainWindow::stageArchitecturalLightingVertex(
    const ArchitecturalLightingVertexSelection& vertex,
    const MotsArchLightVertex& value,
    const level::MotsArchLightingVertexMode mode)
{
    if (!vertex.valid())
    {
        return false;
    }
    const auto selected = std::find_if(
        architecturalLightingInteractive_.selectedVertices.begin(),
        architecturalLightingInteractive_.selectedVertices.end(),
        [&vertex](const auto& candidate)
        {
            return candidate.selection == vertex;
        });
    if (!architecturalLightingInteractive_.active
        || selected == architecturalLightingInteractive_.selectedVertices.end())
    {
        if (!prepareArchitecturalLightingInteractiveSelection({ vertex }))
        {
            return false;
        }
    }
    return stageArchitecturalLightingSelectedVertices(
        value,
        mode,
        std::array<bool, 4>{ true, true, true, true });
}

bool MainWindow::stageArchitecturalLightingSelectedVertices(
    const MotsArchLightVertex& value,
    const level::MotsArchLightingVertexMode mode,
    const std::array<bool, 4>& dirtyChannels)
{
    std::vector<MotsArchLightVertex> values(
        architecturalLightingInteractive_.selectedVertices.size(), value);
    return stageArchitecturalLightingSelectedVertices(
        values, mode, dirtyChannels);
}

bool MainWindow::stageArchitecturalLightingSelectedVertices(
    const std::vector<MotsArchLightVertex>& values,
    const level::MotsArchLightingVertexMode mode,
    const std::array<bool, 4>& dirtyChannels)
{
    ArchitecturalLightingInteractiveState& state =
        architecturalLightingInteractive_;
    if (!state.active || state.selectedVertices.empty()
        || values.size() != state.selectedVertices.size()
        || (mode != level::MotsArchLightingVertexMode::Mono
            && mode != level::MotsArchLightingVertexMode::Rgb)
        || state.documentEpoch != recoveryDocumentEpoch_
        || state.undoSignature != document_.undoSignature()
        || state.resourceIdentity != resources_.previewCacheIdentity()
        || !glView_)
    {
        return false;
    }

    const assets::Templates* templatesDb = templates_.size() > 0
        ? &templates_
        : nullptr;
    std::vector<MotsArchLightVertex> nextValues{};
    std::vector<GlView::ArchitecturalLightingPreviewVertex> preview{};
    nextValues.reserve(state.selectedVertices.size());
    preview.reserve(state.selectedVertices.size());

    std::unordered_map<
        GlView::ArchitecturalLightingVertexIdentity,
        GlView::ArchitecturalLightingRenderVertex,
        GlView::ArchitecturalLightingVertexIdentityHash>
        cachedVertices{};
    std::unordered_set<std::int32_t> cachedOwners{};
    cachedOwners.reserve(state.selectedVertices.size());
    for (const auto& selected : state.selectedVertices)
    {
        const std::int32_t owner = selected.selection.thingIndex;
        if (!cachedOwners.insert(owner).second)
        {
            continue;
        }
        const auto ownerVertices = glView_->architecturalLightingVerticesForThing(
            owner);
        cachedVertices.reserve(cachedVertices.size() + ownerVertices.size());
        for (const auto& candidate : ownerVertices)
        {
            cachedVertices.emplace(
                GlView::ArchitecturalLightingVertexIdentity{
                    candidate.thingIndex,
                    candidate.lodIndex,
                    candidate.meshIndex,
                    candidate.vertexIndex},
                candidate);
        }
    }

    for (std::size_t selectedIndex = 0;
         selectedIndex < state.selectedVertices.size();
         ++selectedIndex)
    {
        const auto& selected = state.selectedVertices[selectedIndex];
        const MotsArchLightVertex& value = values[selectedIndex];
        if (!finiteVertex(value))
        {
            return false;
        }
        const auto& selection = selected.selection;
        if (!selection.valid()
            || static_cast<std::size_t>(selection.thingIndex)
                >= document_.level().things.size()
            || isLayerLocked(document_.level().things[
                static_cast<std::size_t>(selection.thingIndex)].layer)
            || !ui::layers::isLayerVisible(layerVisible_,
                document_.level().things[
                    static_cast<std::size_t>(selection.thingIndex)].layer)
            || !validVertexInTopology(
                selected.topology, selection.meshIndex, selection.vertexIndex))
        {
            return false;
        }
        const Thing& thing = document_.level().things[
            static_cast<std::size_t>(selection.thingIndex)];
        const std::optional<MotsArchLightVertex> currentValue =
            authoredOrDefaultVertex(
                document_.level(), templatesDb, thing,
                selected.sourceRecordIndex, selection.meshIndex,
                selection.vertexIndex);
        if (!currentValue
            || !sameVertexBits(*currentValue, selected.baseline))
        {
            clearArchitecturalLightingInteractiveState();
            return false;
        }

        MotsArchLightVertex next = selected.preview;
        if (dirtyChannels[0])
        {
            next.mono = value.mono;
        }
        if (dirtyChannels[1])
        {
            next.red = value.red;
        }
        if (dirtyChannels[2])
        {
            next.green = value.green;
        }
        if (dirtyChannels[3])
        {
            next.blue = value.blue;
        }
        if (!finiteVertex(next))
        {
            return false;
        }

        const auto cachedIt = cachedVertices.find(
            GlView::ArchitecturalLightingVertexIdentity{
                selection.thingIndex,
                selection.lodIndex,
                selection.meshIndex,
                selection.vertexIndex});
        if (cachedIt == cachedVertices.end())
        {
            return false;
        }
        nextValues.push_back(next);
        preview.push_back(
            GlView::ArchitecturalLightingPreviewVertex{ cachedIt->second, next });
    }

    for (std::size_t index = 0; index < state.selectedVertices.size(); ++index)
    {
        auto& selected = state.selectedVertices[index];
        selected.preview = nextValues[index];
        for (std::size_t channel = 0; channel < dirtyChannels.size(); ++channel)
        {
            selected.dirty[channel] = selected.dirty[channel]
                || dirtyChannels[channel];
        }
    }
    state.vertex = state.selectedVertices.front().selection;
    state.baselineVertex = state.selectedVertices.front().baseline;
    state.previewVertex = state.selectedVertices.front().preview;
    state.previewMode = mode;
    state.previewDirty = std::any_of(
        state.selectedVertices.begin(), state.selectedVertices.end(),
        [](const auto& selected)
        {
            return !sameVertexBits(selected.preview, selected.baseline);
        });
    glView_->setArchitecturalLightingPreview(preview, mode);
    glView_->update();
    return true;
}

bool MainWindow::commitArchitecturalLightingInteractiveEdit()
{
    ArchitecturalLightingInteractiveState& interactive =
        architecturalLightingInteractive_;
    if (!interactive.active || !interactive.vertex
        || interactive.ownerThingIndex < 0)
    {
        return false;
    }
    if (!interactive.previewDirty)
    {
        clearArchitecturalLightingInteractiveState();
        return true;
    }
    if (!documentEditingAllowed()
        || editMode_ != EditMode::light
        || lightEditSubMode_ != LightEditSubMode::architectural
        || document_.level().kind != ProjectType::MOTS
        || interactive.documentEpoch != recoveryDocumentEpoch_
        || interactive.undoSignature != document_.undoSignature()
        || interactive.resourceIdentity != resources_.previewCacheIdentity()
        || !interactive.previewVertex)
    {
        clearArchitecturalLightingInteractiveState();
        return false;
    }

    if (interactive.selectedVertices.size() > 1)
    {
        const std::uint64_t resourceIdentity = interactive.resourceIdentity;
        const std::uint64_t documentEpoch = interactive.documentEpoch;
        const std::uint64_t undoSignature = interactive.undoSignature;
        const level::MotsArchLightingVertexMode mode = interactive.previewMode;
        const auto selectedVertices = interactive.selectedVertices;
        struct OwnerSnapshot
        {
            std::int32_t thingIndex{-1};
            std::int32_t sourceRecordIndex{-1};
            std::string modelIdentity{};
            level::MotsArchLightingTopology topology{};
        };
        std::vector<OwnerSnapshot> ownerSnapshots{};
        ownerSnapshots.reserve(selectedVertices.size());
        for (const auto& selected : selectedVertices)
        {
            const auto owner = std::find_if(
                ownerSnapshots.begin(), ownerSnapshots.end(),
                [&selected](const OwnerSnapshot& candidate)
                {
                    return candidate.thingIndex
                        == selected.selection.thingIndex;
                });
            if (owner == ownerSnapshots.end())
            {
                ownerSnapshots.push_back(OwnerSnapshot{
                    selected.selection.thingIndex,
                    selected.sourceRecordIndex,
                    selected.modelIdentity,
                    selected.topology });
            }
        }
        std::vector<std::int32_t> things{};
        std::vector<std::int32_t> sourceRecords{};
        things.reserve(ownerSnapshots.size());
        sourceRecords.reserve(ownerSnapshots.size());
        for (const OwnerSnapshot& owner : ownerSnapshots)
        {
            things.push_back(owner.thingIndex);
            sourceRecords.push_back(owner.sourceRecordIndex);
        }
        if (things.empty())
        {
            clearArchitecturalLightingInteractiveState();
            return false;
        }
        std::vector<level::MotsArchLightingVertexEdit> vertexEdits{};
        vertexEdits.reserve(selectedVertices.size());
        for (const auto& selected : selectedVertices)
        {
            vertexEdits.push_back(level::MotsArchLightingVertexEdit{
                static_cast<std::size_t>(selected.selection.meshIndex),
                static_cast<std::size_t>(selected.selection.vertexIndex),
                selected.preview,
                selected.dirty,
                selected.selection.thingIndex });
        }
        if (vertexEdits.empty())
        {
            clearArchitecturalLightingInteractiveState();
            return false;
        }
        const assets::Templates* templatesDb = templates_.size() > 0
            ? &templates_
            : nullptr;
        level::MotsArchLightingEditResult editResult{};
        const bool applied = document_.applyUndoable(
            "Things: ARCHLIGHTING vertices changed",
            editor::DocumentDirty::Things,
            [this, &editResult, things, sourceRecords, selectedVertices,
                ownerSnapshots, vertexEdits, mode, resourceIdentity,
                documentEpoch, undoSignature, templatesDb](
                editor::DocumentState& state) -> bool
            {
                if (state.level.kind != ProjectType::MOTS
                    || documentEpoch != recoveryDocumentEpoch_
                    || undoSignature != document_.undoSignature()
                    || resourceIdentity != resources_.previewCacheIdentity())
                {
                    editResult.status = level::MotsArchLightingEditStatus::Rejected;
                    editResult.message =
                        "Architectural lighting selection became stale; choose the vertices again.";
                    return false;
                }
                const auto targets = ui::lighting::collectMotsArchLightingTargetsForThings(
                    state.level, templatesDb, &resources_, things);
                if (targets.size() != things.size())
                {
                    editResult.status = level::MotsArchLightingEditStatus::Rejected;
                    editResult.message =
                        "Architectural lighting model or vertex became stale; choose the vertices again.";
                    return false;
                }
                std::vector<level::MotsArchLightingTopology> topologies{};
                topologies.reserve(things.size());
                for (std::size_t owner = 0; owner < things.size(); ++owner)
                {
                    if (things[owner] < 0
                        || static_cast<std::size_t>(things[owner])
                            >= state.level.things.size())
                    {
                        editResult.status = level::MotsArchLightingEditStatus::Rejected;
                        editResult.message =
                            "Architectural lighting selection became stale; choose the vertices again.";
                        return false;
                    }
                    const auto* target = findTarget(targets, things[owner]);
                    const OwnerSnapshot& expected = ownerSnapshots[owner];
                    if (!target || !target->modelAvailable
                        || !target->valueEditingAllowed
                        || target->resourceRevision != expected.modelIdentity
                        || target->originalRecordIndex != expected.sourceRecordIndex
                        || !sameTopology(target->topology, expected.topology))
                    {
                        editResult.status = level::MotsArchLightingEditStatus::Rejected;
                        editResult.message =
                            "Architectural lighting model or vertex became stale; choose the vertices again.";
                        return false;
                    }
                    const Thing& thing = state.level.things[
                        static_cast<std::size_t>(things[owner])];
                    if (isLayerLocked(thing.layer)
                        || !ui::layers::isLayerVisible(layerVisible_, thing.layer))
                    {
                        editResult.status = level::MotsArchLightingEditStatus::Rejected;
                        editResult.message =
                            "Architectural lighting cannot edit a Thing on a locked or hidden layer.";
                        return false;
                    }
                    topologies.push_back(expected.topology);
                }
                for (const auto& selected : selectedVertices)
                {
                    if (selected.selection.thingIndex < 0
                        || static_cast<std::size_t>(selected.selection.thingIndex)
                            >= state.level.things.size())
                    {
                        editResult.status = level::MotsArchLightingEditStatus::Rejected;
                        editResult.message =
                            "Architectural lighting vertex changed while it was being edited; choose it again.";
                        return false;
                    }
                    const Thing& thing = state.level.things[
                        static_cast<std::size_t>(selected.selection.thingIndex)];
                    const auto currentValue = authoredOrDefaultVertex(
                        state.level, templatesDb, thing,
                        selected.sourceRecordIndex,
                        selected.selection.meshIndex,
                        selected.selection.vertexIndex);
                    if (!currentValue
                        || !sameVertexBits(*currentValue, selected.baseline))
                    {
                        editResult.status = level::MotsArchLightingEditStatus::Rejected;
                        editResult.message =
                            "Architectural lighting vertex changed while it was being edited; choose it again.";
                        return false;
                    }
                }
                level::MotsArchLightingEdit edit{};
                edit.kind = level::MotsArchLightingEditKind::SetVertex;
                edit.mode = mode;
                edit.meshIndex = vertexEdits.front().meshIndex;
                edit.vertexIndex = vertexEdits.front().vertexIndex;
                edit.vertexValue = vertexEdits.front().vertexValue;
                edit.vertexEdits = vertexEdits;
                editResult = level::applyMotsArchLightingEdit(
                    state.level, things, topologies, edit, templatesDb);
                return editResult.status == level::MotsArchLightingEditStatus::Applied;
            });
        if (!applied)
        {
            if (glView_ && !editResult.message.empty())
            {
                glView_->showToast(
                    QString::fromStdString(editResult.message),
                    3200,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::Error);
            }
            return false;
        }
        clearArchitecturalLightingInteractiveState();
        if (glView_)
        {
            glView_->notifyThingAppearanceChanged();
        }
        rebuildOutliner();
        rebuildProperties();
        updateEditActions();
        scheduleRebuildChecks(editor::DocumentDirty::Things);
        return true;
    }

    const std::int32_t thingIndex = interactive.ownerThingIndex;
    if (thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= document_.level().things.size()
        || isLayerLocked(document_.level().things[static_cast<std::size_t>(thingIndex)].layer)
        || !ui::layers::isLayerVisible(layerVisible_,
            document_.level().things[static_cast<std::size_t>(thingIndex)].layer)
        || document_.level().things[static_cast<std::size_t>(thingIndex)].archLightingIndex
            != interactive.sourceRecordIndex)
    {
        clearArchitecturalLightingInteractiveState();
        return false;
    }

    const auto vertex = *interactive.vertex;
    const std::size_t meshIndex = static_cast<std::size_t>(vertex.meshIndex);
    const std::size_t vertexIndex = static_cast<std::size_t>(vertex.vertexIndex);
    if (!interactive.previewVertex)
    {
        clearArchitecturalLightingInteractiveState();
        return false;
    }
    const MotsArchLightVertex value = *interactive.previewVertex;
    const level::MotsArchLightingTopology topology = interactive.topology;
    const std::string modelIdentity = interactive.modelIdentity;
    const std::uint64_t resourceIdentity = interactive.resourceIdentity;
    const std::int32_t sourceRecordIndex = interactive.sourceRecordIndex;
    if (!interactive.baselineVertex)
    {
        clearArchitecturalLightingInteractiveState();
        return false;
    }
    const MotsArchLightVertex baselineVertex = *interactive.baselineVertex;
    const level::MotsArchLightingVertexMode mode = interactive.previewMode;
    const std::array<std::int32_t, 1> things{ thingIndex };
    const std::array<level::MotsArchLightingTopology, 1> topologies{ topology };
    const assets::Templates* templatesDb = templates_.size() > 0
        ? &templates_
        : nullptr;
    level::MotsArchLightingEditResult editResult{};
    const bool applied = document_.applyUndoable(
        "Things: ARCHLIGHTING vertex changed",
        editor::DocumentDirty::Things,
        [this, &editResult, &things, &topologies, value, topology, meshIndex,
            vertexIndex, mode, modelIdentity, resourceIdentity,
            sourceRecordIndex, baselineVertex, templatesDb](
            editor::DocumentState& state) -> bool
        {
            if (state.level.kind != ProjectType::MOTS
                || things[0] < 0
                || static_cast<std::size_t>(things[0]) >= state.level.things.size())
            {
                editResult.status = level::MotsArchLightingEditStatus::Rejected;
                editResult.message =
                    "Architectural lighting selection became stale; choose the Thing again.";
                return false;
            }
            const Thing& thing = state.level.things[
                static_cast<std::size_t>(things[0])];
            if (isLayerLocked(thing.layer)
                || !ui::layers::isLayerVisible(layerVisible_, thing.layer))
            {
                editResult.status = level::MotsArchLightingEditStatus::Rejected;
                editResult.message =
                    "Architectural lighting cannot edit a Thing on a locked or hidden layer.";
                return false;
            }
            if (things[0] < 0
                || static_cast<std::size_t>(things[0]) >= state.level.things.size()
                || state.level.things[static_cast<std::size_t>(things[0])].archLightingIndex
                    != sourceRecordIndex
                || resources_.previewCacheIdentity() != resourceIdentity)
            {
                editResult.status = level::MotsArchLightingEditStatus::Rejected;
                editResult.message =
                    "Architectural lighting selection became stale; choose the Thing again.";
                return false;
            }
            const auto target = ui::lighting::collectMotsArchLightingTarget(
                state.level, templatesDb, &resources_, things[0]);
            if (!target || !target->modelAvailable
                || !target->valueEditingAllowed
                || target->resourceRevision != modelIdentity
                || target->originalRecordIndex != sourceRecordIndex
                || !sameTopology(target->topology, topology)
                || !validVertexInTopology(topology,
                    static_cast<std::int32_t>(meshIndex),
                    static_cast<std::int32_t>(vertexIndex)))
            {
                editResult.status = level::MotsArchLightingEditStatus::Rejected;
                editResult.message =
                    "Architectural lighting model or vertex became stale; choose the Thing again.";
                return false;
            }
            const auto currentValue = authoredOrDefaultVertex(
                state.level, templatesDb, thing, sourceRecordIndex,
                static_cast<std::int32_t>(meshIndex),
                static_cast<std::int32_t>(vertexIndex));
            if (!currentValue
                || !sameVertexBits(*currentValue, baselineVertex))
            {
                editResult.status = level::MotsArchLightingEditStatus::Rejected;
                editResult.message =
                    "Architectural lighting vertex changed while it was being edited; choose it again.";
                return false;
            }
            level::MotsArchLightingEdit edit{};
            edit.kind = level::MotsArchLightingEditKind::SetVertex;
            edit.mode = mode;
            edit.meshIndex = meshIndex;
            edit.vertexIndex = vertexIndex;
            edit.vertexValue = value;
            editResult = level::applyMotsArchLightingEdit(
                state.level, things, topologies, edit, templatesDb);
            return editResult.status == level::MotsArchLightingEditStatus::Applied;
        });
    if (!applied)
    {
        if (glView_ && !editResult.message.empty())
        {
            glView_->showToast(
                QString::fromStdString(editResult.message),
                3200,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    clearArchitecturalLightingInteractiveState();
    if (glView_)
    {
        glView_->notifyThingAppearanceChanged();
    }
    if (!document_.selection().things.empty())
    {
        const std::int32_t owner = glView_ && glView_->activeThingIndex() >= 0
            ? glView_->activeThingIndex()
            : *document_.selection().things.begin();
        (void)refreshArchitecturalLightingInteractiveOwner(owner);
    }
    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
    scheduleRebuildChecks(editor::DocumentDirty::Things);
    return true;
}

void MainWindow::cancelArchitecturalLightingInteractiveEdit() noexcept
{
    clearArchitecturalLightingInteractiveState();
}

bool MainWindow::initializeArchitecturalLightingForSelection()
{
    if (!documentEditingAllowed()
        || editMode_ != EditMode::light
        || lightEditSubMode_ != LightEditSubMode::architectural
        || document_.level().kind != ProjectType::MOTS)
    {
        return false;
    }
    const std::vector<std::int32_t> indices(
        document_.selection().things.begin(), document_.selection().things.end());
    if (indices.empty())
    {
        return false;
    }
    const assets::Templates* templatesDb = templates_.size() > 0
        ? &templates_
        : nullptr;
    const auto targets = ui::lighting::collectMotsArchLightingTargetsForThings(
        document_.level(), templatesDb, &resources_, indices);
    std::vector<level::MotsArchLightingTopology> topologies{};
    topologies.reserve(indices.size());
    for (const std::int32_t index : indices)
    {
        const auto* target = findTarget(targets, index);
        if (!target || !target->modelAvailable
            || target->originalRecordIndex != -1
            || index < 0
            || static_cast<std::size_t>(index) >= document_.level().things.size()
            || (level::thingFlags(
                    document_.level(), templatesDb,
                    document_.level().things[static_cast<std::size_t>(index)])
                & TF_INVISIBLE) != 0
            || isLayerLocked(document_.level().things[static_cast<std::size_t>(index)].layer)
            || !ui::layers::isLayerVisible(layerVisible_,
                document_.level().things[static_cast<std::size_t>(index)].layer))
        {
            return false;
        }
        topologies.push_back(target->topology);
    }

    level::MotsArchLightingEditResult editResult{};
    const bool applied = document_.applyUndoable(
        "Things: ARCHLIGHTING initialized to default",
        editor::DocumentDirty::Things,
        [this, &editResult, indices, topologies, templatesDb](
            editor::DocumentState& state) -> bool
        {
            for (const std::int32_t index : indices)
            {
                if (index < 0
                    || static_cast<std::size_t>(index) >= state.level.things.size()
                    || isLayerLocked(state.level.things[static_cast<std::size_t>(index)].layer)
                    || !ui::layers::isLayerVisible(layerVisible_,
                        state.level.things[static_cast<std::size_t>(index)].layer))
                {
                    editResult.status = level::MotsArchLightingEditStatus::Rejected;
                    editResult.message =
                        "Architectural lighting cannot edit a Thing on a locked or hidden layer.";
                    return false;
                }
            }
            level::MotsArchLightingEdit edit{};
            edit.kind = level::MotsArchLightingEditKind::Initialize;
            edit.mode = level::MotsArchLightingVertexMode::Rgb;
            editResult = level::applyMotsArchLightingEdit(
                state.level, indices, topologies, edit, templatesDb);
            return editResult.status == level::MotsArchLightingEditStatus::Applied;
        });
    if (!applied)
    {
        if (glView_ && !editResult.message.empty())
        {
            glView_->showToast(
                QString::fromStdString(editResult.message),
                3200,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }
    clearArchitecturalLightingInteractiveState();
    if (glView_)
    {
        glView_->notifyThingAppearanceChanged();
    }
    if (!document_.selection().things.empty())
    {
        const std::int32_t owner = glView_ && glView_->activeThingIndex() >= 0
            ? glView_->activeThingIndex()
            : *document_.selection().things.begin();
        (void)refreshArchitecturalLightingInteractiveOwner(owner);
    }
    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
    scheduleRebuildChecks(editor::DocumentDirty::Things);
    return true;
}

bool MainWindow::resetArchitecturalLightingForSelection()
{
    if (!documentEditingAllowed()
        || editMode_ != EditMode::light
        || lightEditSubMode_ != LightEditSubMode::architectural
        || document_.level().kind != ProjectType::MOTS)
    {
        return false;
    }

    const assets::Templates* templatesDb = templates_.size() > 0
        ? &templates_
        : nullptr;
    const Level& currentLevel = document_.level();
    std::vector<std::int32_t> selectedIndices(
        document_.selection().things.begin(), document_.selection().things.end());
    std::sort(selectedIndices.begin(), selectedIndices.end());
    if (selectedIndices.empty())
    {
        return false;
    }

    std::vector<std::int32_t> resetIndices{};
    resetIndices.reserve(selectedIndices.size());
    for (const std::int32_t index : selectedIndices)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= currentLevel.things.size())
        {
            return false;
        }
        const Thing& thing = currentLevel.things[static_cast<std::size_t>(index)];
        if (!level::isMotsArchThing(currentLevel, thing, templatesDb)
            || isLayerLocked(thing.layer)
            || !ui::layers::isLayerVisible(layerVisible_, thing.layer)
            || thing.archLightingIndex < -1
            || (thing.archLightingIndex >= 0
                && static_cast<std::size_t>(thing.archLightingIndex)
                    >= currentLevel.motsArchLighting.size()))
        {
            return false;
        }
        if (thing.archLightingIndex < 0)
        {
            // The effective Arch Thing default is already an all-zero record.
            continue;
        }
        if (!level::isDefaultMotsArchLighting(currentLevel.motsArchLighting[
                static_cast<std::size_t>(thing.archLightingIndex)]))
        {
            resetIndices.push_back(index);
        }
    }
    if (resetIndices.empty())
    {
        return false;
    }

    const auto targets = ui::lighting::collectMotsArchLightingTargetsForThings(
        currentLevel, templatesDb, &resources_, resetIndices);
    std::vector<level::MotsArchLightingTopology> topologies{};
    topologies.reserve(resetIndices.size());
    for (const std::int32_t index : resetIndices)
    {
        const auto* target = findTarget(targets, index);
        if (!target || !target->modelAvailable || target->topology.geosets.empty())
        {
            if (glView_)
            {
                glView_->showToast(
                    QStringLiteral(
                        "Cannot reset ARCHLIGHTING: resolve the Thing's 3DO model first."),
                    3600,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::Error);
            }
            return false;
        }
        topologies.push_back(target->topology);
    }

    level::MotsArchLightingEditResult editResult{};
    const bool applied = document_.applyUndoable(
        "Things: ARCHLIGHTING reset to default",
        editor::DocumentDirty::Things,
        [this, &editResult, resetIndices, topologies, templatesDb](
            editor::DocumentState& state) -> bool
        {
            for (const std::int32_t index : resetIndices)
            {
                if (index < 0
                    || static_cast<std::size_t>(index) >= state.level.things.size()
                    || isLayerLocked(state.level.things[
                        static_cast<std::size_t>(index)].layer)
                    || !ui::layers::isLayerVisible(layerVisible_,
                        state.level.things[static_cast<std::size_t>(index)].layer))
                {
                    editResult.status = level::MotsArchLightingEditStatus::Rejected;
                    editResult.message =
                        "Architectural lighting cannot edit a Thing on a locked or hidden layer.";
                    return false;
                }
            }
            level::MotsArchLightingEdit edit{};
            edit.kind = level::MotsArchLightingEditKind::Reset;
            edit.mode = level::MotsArchLightingVertexMode::Rgb;
            editResult = level::applyMotsArchLightingEdit(
                state.level, resetIndices, topologies, edit, templatesDb);
            return editResult.status == level::MotsArchLightingEditStatus::Applied;
        });
    if (!applied)
    {
        if (glView_ && !editResult.message.empty())
        {
            glView_->showToast(
                QString::fromStdString(editResult.message),
                3600,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }
    clearArchitecturalLightingInteractiveState();
    if (glView_)
    {
        glView_->notifyThingAppearanceChanged();
    }
    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
    scheduleRebuildChecks(editor::DocumentDirty::Things);
    return true;
}

bool MainWindow::copyArchitecturalLightingClipboard()
{
    if (editMode_ != EditMode::light
        || lightEditSubMode_ != LightEditSubMode::architectural
        || document_.level().kind != ProjectType::MOTS)
    {
        return false;
    }

    const editor::Selection& selection = document_.selection();
    std::vector<std::int32_t> indices(
        selection.things.begin(), selection.things.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (indices.empty())
    {
        return false;
    }

    std::int32_t sourceIndex = glView_ ? glView_->activeThingIndex() : -1;
    if (sourceIndex < 0 || !selection.things.contains(sourceIndex))
    {
        sourceIndex = lastViewportClickThing_;
    }
    if (sourceIndex < 0 || !selection.things.contains(sourceIndex))
    {
        return false;
    }
    if (sourceIndex >= static_cast<std::int32_t>(document_.level().things.size())
        || isLayerLocked(document_.level().things[static_cast<std::size_t>(sourceIndex)].layer)
        || !ui::layers::isLayerVisible(layerVisible_,
            document_.level().things[static_cast<std::size_t>(sourceIndex)].layer))
    {
        return false;
    }

    const assets::Templates* templatesDb = templates_.size() > 0
        ? &templates_
        : nullptr;
    const auto targets = ui::lighting::collectMotsArchLightingTargetsForThings(
        document_.level(), templatesDb, &resources_, indices);
    const auto* source = findTarget(targets, sourceIndex);
    if (!source
        || !source->hasArchFlag
        || !source->modelAvailable
        || source->originalRecordIndex < 0
        || source->resourceRevision.empty()
        || source->modelName.empty())
    {
        return false;
    }

    std::string copyError{};
    const auto payload = level::copyMotsArchLightingClipboard(
        document_.level(),
        sourceIndex,
        level::normalizeMotsArchLightingModelIdentity(source->modelName),
        source->topology,
        templatesDb,
        &copyError);
    if (!payload)
    {
        if (glView_ && !copyError.empty())
        {
            glView_->showToast(
                QString::fromStdString(copyError),
                2800,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    std::string encodeError{};
    const auto encoded = level::encodeMotsArchLightingClipboard(
        *payload, &encodeError);
    if (!encoded)
    {
        if (glView_ && !encodeError.empty())
        {
            glView_->showToast(
                QString::fromStdString(encodeError),
                2800,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }
    writeMotsArchLightingClipboard(*payload, *encoded);
    if (QAction* const pasteAction = findChild<QAction*>(
            QStringLiteral("edit.paste_entities")))
    {
        pasteAction->setProperty("sed.clipboard_has_arch_lighting", true);
        if (editMode_ == EditMode::light
            && lightEditSubMode_ == LightEditSubMode::architectural)
        {
            pasteAction->setEnabled(documentEditingAllowed());
        }
    }
    if (glView_)
    {
        glView_->showToast(
            QStringLiteral("Copied ARCHLIGHTING from Thing %1").arg(sourceIndex),
            1800,
            GlView::ToastPlacement::BottomCenter,
            GlView::ToastGroup::General);
    }
    return true;
}

bool MainWindow::pasteArchitecturalLightingClipboard()
{
    if (!documentEditingAllowed()
        || editMode_ != EditMode::light
        || lightEditSubMode_ != LightEditSubMode::architectural
        || document_.level().kind != ProjectType::MOTS)
    {
        return false;
    }

    QString clipboardError{};
    const auto payload = readMotsArchLightingClipboard(clipboardError);
    if (!payload)
    {
        if (glView_ && !clipboardError.isEmpty())
        {
            glView_->showToast(
                QStringLiteral("Paste failed: %1").arg(clipboardError),
                2800,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }

    const editor::Selection& selection = document_.selection();
    std::vector<std::int32_t> indices(
        selection.things.begin(), selection.things.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (indices.empty() || payload->modelIdentity.empty())
    {
        return false;
    }
    const assets::Templates* templatesDb = templates_.size() > 0
        ? &templates_
        : nullptr;
    const auto targets = ui::lighting::collectMotsArchLightingTargetsForThings(
        document_.level(), templatesDb, &resources_, indices);
    std::vector<level::MotsArchLightingClipboardTarget> clipboardTargets{};
    clipboardTargets.reserve(indices.size());
    for (const std::int32_t index : indices)
    {
        if (index < 0
            || static_cast<std::size_t>(index) >= document_.level().things.size()
            || isLayerLocked(document_.level().things[static_cast<std::size_t>(index)].layer)
            || !ui::layers::isLayerVisible(layerVisible_,
                document_.level().things[static_cast<std::size_t>(index)].layer))
        {
            return false;
        }
        const auto* target = findTarget(targets, index);
        if (!target
            || !target->modelAvailable
            || target->resourceRevision.empty()
            || level::normalizeMotsArchLightingModelIdentity(target->modelName)
                != level::normalizeMotsArchLightingModelIdentity(payload->modelIdentity)
            || !sameTopology(target->topology, payload->topology))
        {
            return false;
        }
        if ((level::thingFlags(
                document_.level(), templatesDb,
                document_.level().things[static_cast<std::size_t>(index)])
            & TF_INVISIBLE) != 0)
        {
            return false;
        }
        clipboardTargets.push_back(level::MotsArchLightingClipboardTarget{
            index,
            level::normalizeMotsArchLightingModelIdentity(target->modelName),
            target->topology });
    }

    const std::uint64_t resourceIdentity = resources_.previewCacheIdentity();
    const std::uint64_t undoSignature = document_.undoSignature();
    level::MotsArchLightingClipboardResult pasteResult{};
    const bool applied = document_.applyUndoable(
        "Things: ARCHLIGHTING pasted",
        editor::DocumentDirty::Things,
        [this, payload = *payload, clipboardTargets, resourceIdentity,
            undoSignature, templatesDb, &pasteResult](
            editor::DocumentState& state) -> bool
        {
            if (state.level.kind != ProjectType::MOTS
                || resourceIdentity != resources_.previewCacheIdentity()
                || undoSignature != document_.undoSignature())
            {
                pasteResult.status = level::MotsArchLightingClipboardStatus::Rejected;
                pasteResult.message =
                    "Architectural lighting clipboard became stale; choose the targets again.";
                return false;
            }
            for (const auto& target : clipboardTargets)
            {
                if (target.thingIndex < 0
                    || static_cast<std::size_t>(target.thingIndex)
                        >= state.level.things.size()
                    || isLayerLocked(state.level.things[
                        static_cast<std::size_t>(target.thingIndex)].layer)
                    || !ui::layers::isLayerVisible(layerVisible_,
                        state.level.things[static_cast<std::size_t>(target.thingIndex)].layer))
                {
                    pasteResult.status = level::MotsArchLightingClipboardStatus::Rejected;
                    pasteResult.message =
                        "Architectural lighting cannot edit a Thing on a locked or hidden layer.";
                    return false;
                }
            }
            pasteResult = level::pasteMotsArchLightingClipboard(
                state.level,
                payload,
                clipboardTargets,
                templatesDb);
            return pasteResult.status
                == level::MotsArchLightingClipboardStatus::Applied;
        });
    if (!applied)
    {
        if (pasteResult.status == level::MotsArchLightingClipboardStatus::Noop)
        {
            if (glView_ && !pasteResult.message.empty())
            {
                glView_->showToast(
                    QString::fromStdString(pasteResult.message),
                    1800,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::General);
            }
            return true;
        }
        if (glView_ && !pasteResult.message.empty())
        {
            glView_->showToast(
                QString::fromStdString(pasteResult.message),
                3000,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        return false;
    }
    clearArchitecturalLightingInteractiveState();
    if (glView_)
    {
        glView_->notifyThingAppearanceChanged();
    }
    rebuildOutliner();
    rebuildProperties();
    updateEditActions();
    scheduleRebuildChecks(editor::DocumentDirty::Things);
    return true;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void MainWindow::openArchitecturalLightingVertexEditorForTest(
    const std::int32_t thingIndex,
    const std::int32_t lodIndex,
    const std::int32_t meshIndex,
    const std::int32_t vertexIndex)
{
    openArchitecturalLightingVertexEditor(
        ArchitecturalLightingVertexSelection{
            thingIndex, lodIndex, meshIndex, vertexIndex });
}

void MainWindow::openArchitecturalLightingVertexEditorsForTest(
    const std::vector<std::array<std::int32_t, 4>>& selections)
{
    std::vector<ArchitecturalLightingVertexSelection> converted{};
    converted.reserve(selections.size());
    for (const auto& selection : selections)
    {
        converted.push_back(ArchitecturalLightingVertexSelection{
            selection[0], selection[1], selection[2], selection[3] });
    }
    openArchitecturalLightingVertexEditor(converted);
}

bool MainWindow::copyArchitecturalLightingClipboardForTest()
{
    return copyArchitecturalLightingClipboard();
}

bool MainWindow::pasteArchitecturalLightingClipboardForTest()
{
    return pasteArchitecturalLightingClipboard();
}

QDialog* MainWindow::architecturalLightingVertexDialogForTest() const noexcept
{
    return architecturalLightingVertexDialog_.data();
}
#endif

} // namespace sed
