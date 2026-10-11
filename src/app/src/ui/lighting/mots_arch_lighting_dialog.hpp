#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <QDialog>

#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level/arch_lighting.hpp"

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QString;

namespace sed::ui
{
class ScrubFloatEdit;
} // namespace sed::ui

namespace sed::ui::lighting
{

/**
 * One MotS Thing row exposed by the architectural-lighting editor.
 *
 * A model can expose several renderer geosets, so one topology value carries
 * all ordered geoset prefixes. The row itself contains only metadata; large
 * ARCHLIGHTING payloads are kept once in MotsArchLightingRecordStage below.
 */
struct MotsArchLightingTarget
{
    std::int32_t thingIndex{ -1 };
    std::string label{};
    std::string modelName{};
    std::string resourceRevision{};
    level::MotsArchLightingTopology topology{};
    std::int32_t originalRecordIndex{ -1 };
    std::int32_t recordStageIndex{ -1 };
    bool hasArchFlag{};
    bool modelAvailable{};
    bool valueEditingAllowed{};
};

/**
 * One staged archive payload per distinct source-record index. Targets that
 * share an imported record point at the same stage instead of retaining a
 * full copy per Thing. Rows whose models are unavailable remain visible but
 * cannot stage topology-dependent ARCHLIGHTING value edits.
 */
struct MotsArchLightingRecordStage
{
    std::int32_t originalRecordIndex{ -1 };
    std::optional<MotsArchLightObject> originalRecord{};
    std::optional<MotsArchLightObject> stagedRecord{};
};

/**
 * Bounded state passed from the modal dialog to MainWindow. It owns one
 * compact payload per distinct selected source record and one core edit, not
 * an operation log or a Level snapshot for each numeric field change. One
 * dialog Apply intentionally commits one core edit; changing the operation
 * kind requires Reset before another kind can be staged.
 */
struct MotsArchLightingPlan
{
    ProjectType project{ ProjectType::JKDF2 };
    std::uint64_t resourceLocatorIdentity{};
    std::vector<MotsArchLightingTarget> targets{};
    std::vector<MotsArchLightingRecordStage> records{};
    std::vector<std::int32_t> selectedThingIndices{};
    level::MotsArchLightingEdit edit{};
    bool hasPendingEdit{};
};

enum class MotsArchLightingPlanStatus : std::uint8_t
{
    Applied,
    Noop,
    Rejected,
};

struct MotsArchLightingPlanResult
{
    MotsArchLightingPlanStatus status{ MotsArchLightingPlanStatus::Rejected };
    std::string message{};
};

/** Builds the bounded target snapshot used by the dialog and Apply recheck. */
std::vector<MotsArchLightingTarget> collectMotsArchLightingTargets(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources);

/** Builds selected target metadata with one shared model/LOD topology cache. */
std::vector<MotsArchLightingTarget> collectMotsArchLightingTargetsForThings(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    std::span<const std::int32_t> thingIndices);

/** Builds metadata for one selected Thing without scanning the rest of level. */
std::optional<MotsArchLightingTarget> collectMotsArchLightingTarget(
    const Level& level,
    const assets::Templates* templates,
    const assets::ResourceLocator* resources,
    std::int32_t thingIndex);

/** Returns target topologies in the plan's selected-Thing order. */
std::vector<level::MotsArchLightingTopology>
selectedMotsArchLightingTopologies(const MotsArchLightingPlan& plan);

/**
 * Small, non-Qt staging model shared by the modal dialog and focused tests.
 * Constructor input is bounded to eligible target metadata and deduplicated
 * source records selected for inspection.
 */
class MotsArchLightingStaging final
{
public:
    explicit MotsArchLightingStaging(MotsArchLightingPlan plan);

    const std::vector<MotsArchLightingTarget>& targets() const noexcept;
    const std::vector<MotsArchLightingRecordStage>& records() const noexcept;
    const std::vector<std::int32_t>& selectedThingIndices() const noexcept;
    std::vector<level::MotsArchLightingTopology> selectedTopologies() const;
    const MotsArchLightingPlan& plan() const noexcept;
    bool hasChanges() const noexcept;

    /**
     * Selection is editable before the first pending operation. Once an edit
     * is staged, callers must Reset before changing selected owners so the
     * single core edit cannot silently retarget or discard authored values.
     */
    bool setSelectedThingIndices(std::vector<std::int32_t> thingIndices);

    /**
     * Initialize, Multiply, SetVertex, and ResetToDefault establish or update
     * one operation kind. A different kind while an edit is pending returns
     * false; Clear Staged Edit clears the pending operation.
     */
    bool initializeDefault();
    bool multiply(
        level::MotsArchLightingVertexMode mode,
        float monoMultiplier,
        const ColorF& rgbMultiplier);
    bool setVertex(
        std::size_t meshIndex,
        std::size_t vertexIndex,
        const MotsArchLightVertex& value);
    bool reset();
    bool resetToDefault();

private:
    MotsArchLightingPlan plan_{};
};

/**
 * Rechecks resource identities captured by a dialog before MainWindow calls
 * level::applyMotsArchLightingEdit. Topology-dependent edits, including reset
 * to the all-zero default, require the selected rows to remain compatible.
 */
MotsArchLightingPlanResult validateMotsArchLightingPlanResources(
    const MotsArchLightingPlan& plan,
    const std::vector<MotsArchLightingTarget>& currentTargets);

/**
 * Modal authoring surface. All value edits stay in MotsArchLightingStaging
 * until the dialog is accepted; Cancel leaves the document untouched.
 */
class MotsArchLightingDialog final : public QDialog
{
public:
    MotsArchLightingDialog(
        const Level& level,
        const assets::Templates* templates,
        const assets::ResourceLocator* resources,
        QWidget* parent = nullptr);

    const MotsArchLightingPlan& plan() const noexcept;
    const std::vector<MotsArchLightingTarget>& targets() const noexcept;

private:
    void refreshNumericFields();
    void setStatus(const QString& message);
    void updateApplyState();

    MotsArchLightingStaging staging_;
    QListWidget* targetList_{};
    QSpinBox* meshIndexSpin_{};
    QSpinBox* vertexIndexSpin_{};
    QComboBox* multiplierModeCombo_{};
    ui::ScrubFloatEdit* monoValueEdit_{};
    ui::ScrubFloatEdit* redValueEdit_{};
    ui::ScrubFloatEdit* greenValueEdit_{};
    ui::ScrubFloatEdit* blueValueEdit_{};
    ui::ScrubFloatEdit* monoMultiplierEdit_{};
    ui::ScrubFloatEdit* redMultiplierEdit_{};
    ui::ScrubFloatEdit* greenMultiplierEdit_{};
    ui::ScrubFloatEdit* blueMultiplierEdit_{};
    QPushButton* initializeButton_{};
    QPushButton* multiplyButton_{};
    QPushButton* setVertexButton_{};
    QPushButton* resetValuesButton_{};
    QPushButton* resetButton_{};
    QLabel* statusLabel_{};
    QDialogButtonBox* dialogButtons_{};
};

} // namespace sed::ui::lighting
