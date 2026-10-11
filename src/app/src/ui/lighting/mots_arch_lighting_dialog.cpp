#include "ui/lighting/mots_arch_lighting_dialog.hpp"

#include "sed/assets/3do.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"
#include "thing/thing_resource_utils.hpp"
#include "ui/components/dialog_window_policy.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include <QAbstractItemView>
#include <QAbstractButton>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMargins>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sed::ui::lighting
{
namespace
{

constexpr std::int32_t kArchFlag = 0x00000008;
constexpr std::size_t kMaximumStagedVertices = 1'000'000;

void sizeMotsArchLightingScrub(ui::ScrubFloatEdit* const scrub)
{
    if (!scrub || !scrub->lineEdit())
    {
        return;
    }

    QLineEdit* const lineEdit = scrub->lineEdit();
    const QFontMetrics metrics(lineEdit->font());
    // ARCHLIGHTING stores finite float payloads without a UI range. Reserve
    // a signed, nine-decimal HDR example so hover steppers never take digits
    // from the authored value. Larger values still scroll normally in the
    // line edit; this is the readable baseline used by the other scrub tools.
    const int valueWidth = metrics.horizontalAdvance(
        QStringLiteral("-1000000.000000000"));
    const QMargins margins = lineEdit->textMargins();
    const int frameWidth = lineEdit->style()->pixelMetric(
        QStyle::PM_DefaultFrameWidth, nullptr, lineEdit);

    int stepperWidth = 0;
    const QList<QToolButton*> steppers = scrub->findChildren<QToolButton*>();
    for (QToolButton* const stepper : steppers)
    {
        if (stepper)
        {
            // ScrubFloatEdit initially sizes its arrows from the platform
            // button hint.  That hint includes a full push-button margin on
            // some styles, which can make the arrows wider than the numeric
            // control needs. Keep the actual glyph and a DPI/font-scaled
            // touch margin, while retaining enough room for the style frame.
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
            stepperWidth = std::max(stepperWidth, compactWidth);
        }
    }

    const int textWidth = valueWidth + metrics.averageCharWidth();
    const int lineEditWidth = textWidth
        + margins.left() + margins.right() + 2 * frameWidth + 2;
    scrub->setFixedWidth(lineEditWidth + 2 * stepperWidth);
}

struct ModelTopologies
{
    std::array<std::optional<level::MotsArchLightingTopology>, 4> lods{};
    bool loaded{};
};

std::optional<level::MotsArchLightingTopology> topologyFor(
    const assets::Model3do& model);

void loadModelTopologies(
    const assets::ResourceLocator* resources,
    const std::string& modelName,
    ModelTopologies& result)
{
    if (result.loaded)
    {
        return;
    }
    result.loaded = true;
    if (!resources || modelName.empty())
    {
        return;
    }

    try
    {
        const auto bytes = resources->tryReadBinaryWithExt(modelName, ".3do");
        if (!bytes)
        {
            return;
        }

        bool parseFailed = false;
        for (std::int32_t lod = 0; lod < 4; ++lod)
        {
            try
            {
                const assets::Model3do model = assets::loadFrom3DO(
                    std::span<const std::byte>(*bytes),
                    // ARCHLIGHTING topology is authored in source-mesh order.
                    // Do not expand hierarchy instances here: the renderer
                    // maps transformed clones back to these source ordinals.
                    assets::Model3doLoadOptions{ lod, false });
                if (const auto topology = topologyFor(model))
                {
                    result.lods[static_cast<std::size_t>(lod)] = *topology;
                }
            }
            catch (const std::exception&)
            {
                parseFailed = true;
                break;
            }
        }
        if (parseFailed)
        {
            result.lods = {};
        }
    }
    catch (const std::exception&)
    {
        // Missing or malformed resources stay unavailable for this snapshot.
    }
}

std::optional<level::MotsArchLightingTopology> topologyFor(
    const assets::Model3do& model)
{
    if (model.meshes.empty())
    {
        return std::nullopt;
    }
    level::MotsArchLightingTopology topology{};
    std::vector<std::size_t> counts{};
    counts.reserve(model.meshes.size());
    for (const auto& mesh : model.meshes)
    {
        counts.push_back(mesh.vertices.size());
    }
    topology.geosets.push_back(std::move(counts));
    return topology;
}

std::optional<MotsArchLightingTarget> makeTarget(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const std::size_t index,
    std::unordered_map<std::string, ModelTopologies>& models)
{
    if (level.kind != ProjectType::MOTS || index >= level.things.size())
    {
        return std::nullopt;
    }

    const Thing& thing = level.things[index];
    const std::int32_t flags = level::thingFlags(level, templates, thing);
    const bool hasArchFlag = (flags & kArchFlag) != 0;
    const bool invisible = (flags & TF_INVISIBLE) != 0;

    MotsArchLightingTarget target{};
    target.thingIndex = static_cast<std::int32_t>(index);
    target.label = thing.name.empty()
        ? "Thing " + std::to_string(index)
        : thing.name + " (" + std::to_string(index) + ")";
    target.modelName = thing::thingModel3doName(level, templates, thing);
    if (!hasArchFlag
        && thing.archLightingIndex == -1
        && target.modelName.empty())
    {
        return std::nullopt;
    }
    target.resourceRevision = detail::previewResourceRevisionToken(
        resources, target.modelName, ".3do");
    target.hasArchFlag = hasArchFlag;
    target.originalRecordIndex = thing.archLightingIndex;

    // An invisible unbound model is not an initialization candidate. Existing
    // rows remain discoverable so the designer can identify stale authored data.
    if (invisible && thing.archLightingIndex == -1)
    {
        return std::nullopt;
    }

    if (!target.modelName.empty())
    {
        ModelTopologies& model = models[target.modelName];
        loadModelTopologies(resources, target.modelName, model);
        for (const auto& topology : model.lods)
        {
            if (!topology)
            {
                continue;
            }
            target.modelAvailable = true;
            target.topology.geosets.insert(
                target.topology.geosets.end(),
                topology->geosets.begin(), topology->geosets.end());
        }
    }

    target.valueEditingAllowed = !invisible
        && target.modelAvailable
        && (target.originalRecordIndex == -1
            || (target.hasArchFlag
                && static_cast<std::size_t>(target.originalRecordIndex)
                    < level.motsArchLighting.size()));
    return target;
}

MotsArchLightingPlan planForDialog(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources)
{
    MotsArchLightingPlan plan{};
    plan.project = level.kind;
    plan.resourceLocatorIdentity = resources ? resources->previewCacheIdentity() : 0;
    plan.targets = collectMotsArchLightingTargets(level, templates, resources);

    std::unordered_map<std::int32_t, std::int32_t> stages{};
    stages.reserve(plan.targets.size());
    std::size_t stagedMeshes = 0;
    std::size_t stagedVertices = 0;
    for (MotsArchLightingTarget& target : plan.targets)
    {
        target.recordStageIndex = -1;
        if (target.originalRecordIndex < 0
            || static_cast<std::size_t>(target.originalRecordIndex)
                >= level.motsArchLighting.size()
            || !target.valueEditingAllowed)
        {
            continue;
        }

        if (const auto found = stages.find(target.originalRecordIndex);
            found != stages.end())
        {
            target.recordStageIndex = found->second;
            continue;
        }

        const MotsArchLightObject& source = level.motsArchLighting[
            static_cast<std::size_t>(target.originalRecordIndex)];
        std::size_t sourceVertices = 0;
        bool withinBudget = true;
        for (const MotsArchLightMesh& mesh : source.meshes)
        {
            if (mesh.vertices.size() > kMaximumStagedVertices - sourceVertices)
            {
                withinBudget = false;
                break;
            }
            sourceVertices += mesh.vertices.size();
        }
        if (!withinBudget
            || source.meshes.size() > kMaximumStagedVertices - stagedMeshes
            || sourceVertices > kMaximumStagedVertices - stagedVertices)
        {
            target.valueEditingAllowed = false;
            continue;
        }

        MotsArchLightingRecordStage stage{};
        stage.originalRecordIndex = target.originalRecordIndex;
        stage.originalRecord = source;
        stage.stagedRecord = source;
        const std::int32_t stageIndex = static_cast<std::int32_t>(plan.records.size());
        plan.records.push_back(std::move(stage));
        stages.emplace(target.originalRecordIndex, stageIndex);
        target.recordStageIndex = stageIndex;
        stagedMeshes += source.meshes.size();
        stagedVertices += sourceVertices;
    }
    plan.edit.mode = level::MotsArchLightingVertexMode::Rgb;
    plan.edit.rgbMultiplier = ColorWhite;
    return plan;
}

MotsArchLightingTarget* targetFor(
    MotsArchLightingPlan& plan,
    const std::int32_t thingIndex)
{
    const auto it = std::find_if(
        plan.targets.begin(), plan.targets.end(),
        [thingIndex](const MotsArchLightingTarget& target)
        {
            return target.thingIndex == thingIndex;
        });
    return it == plan.targets.end() ? nullptr : &*it;
}

const MotsArchLightingTarget* targetFor(
    const MotsArchLightingPlan& plan,
    const std::int32_t thingIndex)
{
    const auto it = std::find_if(
        plan.targets.begin(), plan.targets.end(),
        [thingIndex](const MotsArchLightingTarget& target)
        {
            return target.thingIndex == thingIndex;
        });
    return it == plan.targets.end() ? nullptr : &*it;
}

const MotsArchLightingRecordStage* stageFor(
    const MotsArchLightingPlan& plan,
    const MotsArchLightingTarget& target)
{
    if (target.recordStageIndex < 0
        || static_cast<std::size_t>(target.recordStageIndex) >= plan.records.size())
    {
        return nullptr;
    }
    return &plan.records[static_cast<std::size_t>(target.recordStageIndex)];
}


} // namespace

std::vector<MotsArchLightingTarget> collectMotsArchLightingTargets(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources)
{
    std::vector<MotsArchLightingTarget> targets{};
    if (level.kind != ProjectType::MOTS)
    {
        return targets;
    }

    std::unordered_map<std::string, ModelTopologies> models{};

    for (std::size_t index = 0; index < level.things.size(); ++index)
    {
        const auto target = makeTarget(level, templates, resources, index, models);
        if (!target)
        {
            continue;
        }
        targets.push_back(*target);
    }

    return targets;
}

std::vector<MotsArchLightingTarget> collectMotsArchLightingTargetsForThings(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const std::span<const std::int32_t> thingIndices)
{
    std::vector<MotsArchLightingTarget> targets{};
    if (level.kind != ProjectType::MOTS)
    {
        return targets;
    }

    std::unordered_map<std::string, ModelTopologies> models{};
    targets.reserve(thingIndices.size());
    for (const std::int32_t thingIndex : thingIndices)
    {
        if (thingIndex < 0)
        {
            continue;
        }
        const auto target = makeTarget(
            level,
            templates,
            resources,
            static_cast<std::size_t>(thingIndex),
            models);
        if (target)
        {
            targets.push_back(*target);
        }
    }
    return targets;
}

std::optional<MotsArchLightingTarget> collectMotsArchLightingTarget(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    const std::int32_t thingIndex)
{
    if (thingIndex < 0)
    {
        return std::nullopt;
    }
    std::unordered_map<std::string, ModelTopologies> models{};
    return makeTarget(
        level,
        templates,
        resources,
        static_cast<std::size_t>(thingIndex),
        models);
}

std::vector<level::MotsArchLightingTopology>
selectedMotsArchLightingTopologies(const MotsArchLightingPlan& plan)
{
    std::vector<level::MotsArchLightingTopology> result{};
    result.reserve(plan.selectedThingIndices.size());
    for (const std::int32_t thingIndex : plan.selectedThingIndices)
    {
        if (const auto* target = targetFor(plan, thingIndex))
        {
            result.push_back(target->topology);
        }
    }
    return result;
}

MotsArchLightingDialog::MotsArchLightingDialog(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    QWidget* parent)
    : QDialog(parent),
      staging_(planForDialog(level, templates, resources))
{
    setObjectName(QStringLiteral("motsArchLightingDialog"));
    setWindowTitle(QStringLiteral("Arch lighting"));
    applyFixedDialogWindowPolicy(*this);
    // Scope the compact-arrow rule to this dialog. Other ScrubFloatEdit
    // users retain their own sizing and interaction policies.
    setStyleSheet(QStringLiteral(
        "QToolButton[sedButtonRole=\"stepper\"] {"
        " min-width: 0px; padding: 0px 2px;"
        " }"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);
    auto* description = new QLabel(
        QStringLiteral("Edit MotS ARCHLIGHTING for Arch Things. Apply commits one edit in one undo step."),
        this);
    description->setObjectName(QStringLiteral("motsArchLightingDescription"));
    description->setWordWrap(true);
    root->addWidget(description);

    auto* targetsBox = new QGroupBox(QStringLiteral("Arch Things"), this);
    targetsBox->setObjectName(QStringLiteral("motsArchLightingTargetsGroup"));
    auto* targetsLayout = new QVBoxLayout(targetsBox);
    targetList_ = new QListWidget(targetsBox);
    targetList_->setObjectName(QStringLiteral("motsArchLightingTargets"));
    targetList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    for (const auto& target : staging_.targets())
    {
        QString text = QString::fromStdString(target.label);
        if (!target.modelAvailable)
        {
            text += QStringLiteral("  [model unavailable]");
        }
        if (target.originalRecordIndex == -1)
        {
            text += QStringLiteral("  [unbound]");
        }
        else if (target.originalRecordIndex < -1)
        {
            text += QStringLiteral("  [invalid binding]");
        }
        auto* item = new QListWidgetItem(text, targetList_);
        item->setData(Qt::UserRole, target.thingIndex);
    }
    targetsLayout->addWidget(targetList_);
    root->addWidget(targetsBox, 1);

    auto* valuesBox = new QGroupBox(QStringLiteral("Vertex values and multipliers"), this);
    auto* valuesLayout = new QGridLayout(valuesBox);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Mesh"), valuesBox), 0, 0);
    meshIndexSpin_ = new QSpinBox(valuesBox);
    meshIndexSpin_->setObjectName(QStringLiteral("motsArchLightingMesh"));
    meshIndexSpin_->setRange(0, 0);
    valuesLayout->addWidget(meshIndexSpin_, 0, 1);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Vertex"), valuesBox), 0, 2);
    vertexIndexSpin_ = new QSpinBox(valuesBox);
    vertexIndexSpin_->setObjectName(QStringLiteral("motsArchLightingVertex"));
    vertexIndexSpin_->setRange(0, 0);
    valuesLayout->addWidget(vertexIndexSpin_, 0, 3);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Multiplier mode"), valuesBox), 1, 0);
    multiplierModeCombo_ = new QComboBox(valuesBox);
    multiplierModeCombo_->setObjectName(QStringLiteral("motsArchLightingMode"));
    multiplierModeCombo_->addItem(QStringLiteral("RGB"), static_cast<int>(level::MotsArchLightingVertexMode::Rgb));
    multiplierModeCombo_->addItem(QStringLiteral("Mono"), static_cast<int>(level::MotsArchLightingVertexMode::Mono));
    valuesLayout->addWidget(multiplierModeCombo_, 1, 1, 1, 3);

    const auto makeValueEdit = [this](const char* name)
    {
        auto* edit = new ui::ScrubFloatEdit(this);
        edit->setObjectName(QString::fromLatin1(name));
        edit->clearRange();
        edit->setDecimals(9);
        edit->setSingleStep(0.01);
        sizeMotsArchLightingScrub(edit);
        return edit;
    };
    monoValueEdit_ = makeValueEdit("motsArchLightingMono");
    redValueEdit_ = makeValueEdit("motsArchLightingRed");
    greenValueEdit_ = makeValueEdit("motsArchLightingGreen");
    blueValueEdit_ = makeValueEdit("motsArchLightingBlue");
    valuesLayout->addWidget(new QLabel(QStringLiteral("Mono"), valuesBox), 2, 0);
    valuesLayout->addWidget(monoValueEdit_, 2, 1);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Red"), valuesBox), 2, 2);
    valuesLayout->addWidget(redValueEdit_, 2, 3);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Green"), valuesBox), 3, 0);
    valuesLayout->addWidget(greenValueEdit_, 3, 1);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Blue"), valuesBox), 3, 2);
    valuesLayout->addWidget(blueValueEdit_, 3, 3);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Mono multiplier"), valuesBox), 4, 0);
    monoMultiplierEdit_ = makeValueEdit("motsArchLightingMonoMultiplier");
    monoMultiplierEdit_->setValue(1.0);
    valuesLayout->addWidget(monoMultiplierEdit_, 4, 1);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Red multiplier"), valuesBox), 4, 2);
    redMultiplierEdit_ = makeValueEdit("motsArchLightingRedMultiplier");
    redMultiplierEdit_->setValue(1.0);
    valuesLayout->addWidget(redMultiplierEdit_, 4, 3);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Green multiplier"), valuesBox), 5, 0);
    greenMultiplierEdit_ = makeValueEdit("motsArchLightingGreenMultiplier");
    greenMultiplierEdit_->setValue(1.0);
    valuesLayout->addWidget(greenMultiplierEdit_, 5, 1);
    valuesLayout->addWidget(new QLabel(QStringLiteral("Blue multiplier"), valuesBox), 5, 2);
    blueMultiplierEdit_ = makeValueEdit("motsArchLightingBlueMultiplier");
    blueMultiplierEdit_->setValue(1.0);
    valuesLayout->addWidget(blueMultiplierEdit_, 5, 3);
    root->addWidget(valuesBox);

    auto* operationLayout = new QHBoxLayout();
    initializeButton_ = new QPushButton(QStringLiteral("Initialize default"), this);
    initializeButton_->setObjectName(QStringLiteral("motsArchLightingInitialize"));
    multiplyButton_ = new QPushButton(QStringLiteral("Multiply"), this);
    multiplyButton_->setObjectName(QStringLiteral("motsArchLightingMultiply"));
    setVertexButton_ = new QPushButton(QStringLiteral("Set vertex"), this);
    setVertexButton_->setObjectName(QStringLiteral("motsArchLightingSetVertex"));
    resetValuesButton_ = new QPushButton(
        QStringLiteral("Reset values to default"), this);
    resetValuesButton_->setObjectName(
        QStringLiteral("motsArchLightingResetValues"));
    resetButton_ = new QPushButton(QStringLiteral("Clear staged edit"), this);
    resetButton_->setObjectName(QStringLiteral("motsArchLightingReset"));
    for (QPushButton* button : { initializeButton_, multiplyButton_, setVertexButton_, resetValuesButton_, resetButton_ })
    {
        operationLayout->addWidget(button);
    }
    root->addLayout(operationLayout);
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("motsArchLightingStatus"));
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    dialogButtons_ = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    dialogButtons_->setObjectName(QStringLiteral("motsArchLightingDialogButtons"));
    root->addWidget(dialogButtons_);

    const auto selectedIndices = [this]()
    {
        std::vector<std::int32_t> indices{};
        if (!targetList_)
        {
            return indices;
        }
        for (const QListWidgetItem* item : targetList_->selectedItems())
        {
            indices.push_back(item->data(Qt::UserRole).toInt());
        }
        return indices;
    };
    connect(targetList_, &QListWidget::itemSelectionChanged, this, [this, selectedIndices]
        {
            if (!staging_.setSelectedThingIndices(selectedIndices()))
            {
                setStatus(QStringLiteral("Reset the pending edit before changing selected Things."));
                return;
            }
            refreshNumericFields();
            updateApplyState();
        });
    connect(meshIndexSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
        [this](int) { refreshNumericFields(); });
    connect(vertexIndexSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
        [this](int) { refreshNumericFields(); });
    connect(initializeButton_, &QPushButton::clicked, this, [this]
        {
            if (!staging_.initializeDefault())
            {
                setStatus(QStringLiteral("Select only unbound model-backed Arch Things."));
            }
            else
            {
                setStatus(QStringLiteral("Default ARCHLIGHTING staged. Apply to commit it."));
            }
            updateApplyState();
        });
    connect(multiplyButton_, &QPushButton::clicked, this, [this]
        {
            const auto mode = static_cast<level::MotsArchLightingVertexMode>(
                multiplierModeCombo_->currentData().toInt());
            const bool applied = staging_.multiply(
                mode,
                static_cast<float>(monoMultiplierEdit_->value()),
                ColorF{
                    static_cast<float>(redMultiplierEdit_->value()),
                    static_cast<float>(greenMultiplierEdit_->value()),
                    static_cast<float>(blueMultiplierEdit_->value()),
                    1.0f });
            setStatus(applied
                ? QStringLiteral("Multiplier staged. Apply to commit it.")
                : QStringLiteral("Reset the pending edit before changing operation or selection."));
            updateApplyState();
        });
    connect(setVertexButton_, &QPushButton::clicked, this, [this]
        {
            const bool applied = staging_.setVertex(
                static_cast<std::size_t>(meshIndexSpin_->value()),
                static_cast<std::size_t>(vertexIndexSpin_->value()),
                MotsArchLightVertex{
                    static_cast<float>(monoValueEdit_->value()),
                    static_cast<float>(redValueEdit_->value()),
                    static_cast<float>(greenValueEdit_->value()),
                    static_cast<float>(blueValueEdit_->value()) });
            setStatus(applied
                ? QStringLiteral("Vertex Set staged. Apply to commit it.")
                : QStringLiteral("Set requires a compatible model and the same vertex until Reset."));
            updateApplyState();
        });
    connect(resetValuesButton_, &QPushButton::clicked, this, [this]
        {
            const bool applied = staging_.resetToDefault();
            setStatus(applied
                ? QStringLiteral("Reset to default staged. Apply to commit it.")
                : QStringLiteral("Reset requires compatible ARCHLIGHTING models attached to Arch Things."));
            updateApplyState();
        });
    connect(resetButton_, &QPushButton::clicked, this, [this]
        {
            if (staging_.reset())
            {
                setStatus(QStringLiteral("Pending edit reset."));
            }
            refreshNumericFields();
            updateApplyState();
        });
    connect(dialogButtons_->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, [this]
        {
            if (!staging_.hasChanges())
            {
                setStatus(QStringLiteral("No changes are staged."));
                return;
            }
            accept();
        });
    connect(dialogButtons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (targetList_->count() > 0)
    {
        targetList_->item(0)->setSelected(true);
    }
    refreshNumericFields();
    setStatus(QStringLiteral("Choose an operation, then Apply once."));
    updateApplyState();
    adjustSize();
}

const MotsArchLightingPlan& MotsArchLightingDialog::plan() const noexcept
{
    return staging_.plan();
}

const std::vector<MotsArchLightingTarget>&
MotsArchLightingDialog::targets() const noexcept
{
    return staging_.targets();
}

void MotsArchLightingDialog::setStatus(const QString& message)
{
    if (statusLabel_)
    {
        statusLabel_->setText(message);
    }
}

void MotsArchLightingDialog::refreshNumericFields()
{
    if (!meshIndexSpin_ || !vertexIndexSpin_)
    {
        return;
    }
    std::size_t meshCount = 0;
    const auto topologies = staging_.selectedTopologies();
    for (const auto& topology : topologies)
    {
        for (const auto& geoset : topology.geosets)
        {
            meshCount = std::max(meshCount, geoset.size());
        }
    }
    const QSignalBlocker meshSignals(*meshIndexSpin_);
    const QSignalBlocker vertexSignals(*vertexIndexSpin_);
    const int maximumMesh = meshCount == 0
        ? 0
        : static_cast<int>(std::min<std::size_t>(
            meshCount - 1, std::numeric_limits<int>::max()));
    meshIndexSpin_->setRange(0, maximumMesh);
    const int meshIndex = std::clamp(
        meshIndexSpin_->value(), 0, maximumMesh);
    meshIndexSpin_->setValue(meshIndex);

    std::size_t vertexCountForMesh = 0;
    for (const auto& topology : topologies)
    {
        for (const auto& geoset : topology.geosets)
        {
            if (static_cast<std::size_t>(meshIndex) < geoset.size())
            {
                vertexCountForMesh = std::max(
                    vertexCountForMesh,
                    geoset[static_cast<std::size_t>(meshIndex)]);
            }
        }
    }
    const int maximumVertex = vertexCountForMesh == 0
        ? 0
        : static_cast<int>(std::min<std::size_t>(
            vertexCountForMesh - 1, std::numeric_limits<int>::max()));
    vertexIndexSpin_->setRange(0, maximumVertex);
    const int vertexIndex = std::clamp(
        vertexIndexSpin_->value(), 0, maximumVertex);
    vertexIndexSpin_->setValue(vertexIndex);

    MotsArchLightVertex value{ 1.0f, 1.0f, 1.0f, 1.0f };
    if (!staging_.selectedThingIndices().empty())
    {
        if (const auto* target = targetFor(
                staging_.plan(), staging_.selectedThingIndices().front()))
        {
            if (const auto* stage = stageFor(
                    staging_.plan(), *target);
                stage && stage->stagedRecord
                && static_cast<std::size_t>(meshIndex)
                    < stage->stagedRecord->meshes.size()
                && static_cast<std::size_t>(vertexIndex)
                    < stage->stagedRecord->meshes[
                        static_cast<std::size_t>(meshIndex)].vertices.size())
            {
                value = stage->stagedRecord->meshes[
                    static_cast<std::size_t>(meshIndex)].vertices[
                        static_cast<std::size_t>(vertexIndex)];
            }
        }
    }
    monoValueEdit_->setValue(value.mono);
    redValueEdit_->setValue(value.red);
    greenValueEdit_->setValue(value.green);
    blueValueEdit_->setValue(value.blue);
}

void MotsArchLightingDialog::updateApplyState()
{
    const bool pending = staging_.plan().hasPendingEdit;
    if (dialogButtons_ && dialogButtons_->button(QDialogButtonBox::Apply))
    {
        dialogButtons_->button(QDialogButtonBox::Apply)->setEnabled(staging_.hasChanges());
    }
    if (targetList_)
    {
        targetList_->setEnabled(!pending);
    }
    if (meshIndexSpin_)
    {
        meshIndexSpin_->setEnabled(!pending);
    }
    if (vertexIndexSpin_)
    {
        vertexIndexSpin_->setEnabled(!pending);
    }
    if (resetButton_)
    {
        resetButton_->setEnabled(pending);
    }
}

} // namespace sed::ui::lighting
