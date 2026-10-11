#include "main_window/common.hpp"
#include "ui/query/level_query_utils.hpp"
#include "ui/query/level_query_numeric_text.hpp"

namespace sed
{

using namespace ::sed::ui::query;

namespace
{

class LevelQueryDialog final : public QDialog
{
public:
    explicit LevelQueryDialog(
        QWidget* parent,
        const std::array<std::optional<LevelQuerySpec>, 3>& initialSpecs,
        const LevelQueryEntityKind initialKind,
        const std::optional<LevelQueryEntityKind> forcedKind,
        std::shared_ptr<const assets::ResourceLocator> resources,
        const assets::Templates* templatesDb,
        const Level& level,
        std::optional<std::filesystem::path> preferredDiskRoot,
        std::optional<std::filesystem::path> projectDirectory,
        const std::vector<ThingTemplate>* templateOverrides)
        : QDialog(parent)
        , rememberedSpecs_(initialSpecs)
        , resources_(std::move(resources))
        , templatesDb_(templatesDb)
        , level_(level)
        , preferredDiskRoot_(std::move(preferredDiskRoot))
        , projectDirectory_(std::move(projectDirectory))
        , templateOverrides_(templateOverrides)
        , forcedKind_(forcedKind)
    {
        setWindowTitle(levelQueryDialogTitle(forcedKind_));
        setModal(true);
        setMinimumWidth(420);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(10);

        auto* intro = new QLabel(
            levelQueryDialogIntro(forcedKind_),
            this);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        auto* form = new QFormLayout();
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setContentsMargins(0, 0, 0, 0);
        form->setSpacing(8);
        layout->addLayout(form);

        kindCombo_ = new QComboBox(this);
        kindCombo_->addItem(QStringLiteral("Sectors"), static_cast<int>(LevelQueryEntityKind::sector));
        kindCombo_->addItem(QStringLiteral("Surfaces"), static_cast<int>(LevelQueryEntityKind::surface));
        kindCombo_->addItem(QStringLiteral("Things"), static_cast<int>(LevelQueryEntityKind::thing));
        kindCombo_->setVisible(!forcedKind_.has_value());

        criteriaPanel_ = new QWidget(this);
        criteriaLayout_ = new QVBoxLayout(criteriaPanel_);
        criteriaLayout_->setContentsMargins(0, 0, 0, 0);
        criteriaLayout_->setSpacing(6);
        form->addRow(QStringLiteral("Criteria"), criteriaPanel_);

        addCriterionButton_ = new QPushButton(QStringLiteral("Add criterion"), this);
        addCriterionButton_->setToolTip(QStringLiteral("Add another field filter for this query."));
        addCriterionButton_->setAutoDefault(false);
        addCriterionButton_->setDefault(false);
        layout->addWidget(addCriterionButton_, 0, Qt::AlignLeft);
        addCriterionButton_->setVisible(!forcedKind_.has_value());

        caseSensitiveCheck_ = new QCheckBox(QStringLiteral("Case-sensitive"), this);
        layout->addWidget(caseSensitiveCheck_);

        resultModeCombo_ = new QComboBox(this);
        resultModeCombo_->addItem(QStringLiteral("Focus in editor"), static_cast<int>(LevelQueryResultMode::focus));
        resultModeCombo_->addItem(QStringLiteral("Add to selection"), static_cast<int>(LevelQueryResultMode::add));
        resultModeCombo_->addItem(QStringLiteral("Subtract from selection"), static_cast<int>(LevelQueryResultMode::subtract));
        form->addRow(QStringLiteral("Result"), resultModeCombo_);

        selectionOnlyCheck_ = new QCheckBox(QStringLiteral("Search current selection only"), this);
        layout->addWidget(selectionOnlyCheck_);

        auto* buttons = new QDialogButtonBox(this);
        findButton_ = buttons->addButton(QStringLiteral("Find"), QDialogButtonBox::AcceptRole);
        selectAllButton_ = buttons->addButton(QStringLiteral("Select All"), QDialogButtonBox::ActionRole);
        findButton_->setAutoDefault(false);
        findButton_->setDefault(false);
        selectAllButton_->setAutoDefault(false);
        selectAllButton_->setDefault(false);
        buttons->addButton(QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        const LevelQueryEntityKind effectiveInitialKind = forcedKind_.value_or(initialKind);
        kindCombo_->setCurrentIndex(std::max(0, kindCombo_->findData(static_cast<int>(effectiveInitialKind))));
        applySpecToUi(
            rememberedSpecs_[levelQueryEntityKindIndex(effectiveInitialKind)].value_or(
                makeDefaultSpec(effectiveInitialKind)));

        connect(addCriterionButton_, &QPushButton::clicked, this, [this]()
            {
                addCriterionRow(std::nullopt);
            });
        connect(findButton_, &QPushButton::clicked, this, [this]()
            {
                selectAll_ = false;
                accept();
            });
        connect(selectAllButton_, &QPushButton::clicked, this, [this]()
            {
                selectAll_ = true;
                accept();
            });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        ui::onUiNumericFormatChanged(this, [this]()
        {
            for (CriterionRow* row : rows_)
            {
                updateRowPlaceholder(row);
                if (!row || !row->valueEdit || row->valueEdit->isModified())
                {
                    continue;
                }
                const LevelQueryField field = rowField(row);
                const LevelQueryMatchMode mode = levelQueryMatchModeFromInt(
                    row->matchCombo->currentData().toInt());
                const auto canonical = canonicalLevelQueryNumericText(
                    row->valueEdit->text(), field, mode, row->valueEdit->locale());
                if (canonical)
                {
                    const QSignalBlocker blocker(row->valueEdit);
                    row->valueEdit->setLocale(ui::uiNumericLocale());
                    row->valueEdit->setText(localizeLevelQueryNumericText(*canonical, field, mode));
                    row->valueEdit->setModified(false);
                }
            }
            updateButtonState();
        });
    }

    ~LevelQueryDialog() override
    {
        for (CriterionRow* row : rows_)
        {
            delete row;
        }
        rows_.clear();
    }

    LevelQuerySpec spec() const
    {
        storeCurrentUiSpec();
        const LevelQueryEntityKind kind = currentKind();
        return rememberedSpecs_[static_cast<std::size_t>(levelQueryEntityKindIndex(kind))].value_or(makeDefaultSpec(kind));
    }

    bool selectAllRequested() const noexcept
    {
        return selectAll_;
    }

    std::array<std::optional<LevelQuerySpec>, 3> rememberedSpecs() const
    {
        storeCurrentUiSpec();
        return rememberedSpecs_;
    }

private:
    struct CriterionRow
    {
        QWidget* container{};
        QLabel* fieldLabel{};
        QComboBox* fieldCombo{};
        LevelQueryField field{LevelQueryField::surfaceMaterial};
        QComboBox* matchCombo{};
        QLineEdit* valueEdit{};
        QPushButton* browseButton{};
        QPushButton* removeButton{};
    };

    static LevelQuerySpec makeDefaultSpec(const LevelQueryEntityKind kind)
    {
        LevelQuerySpec spec{};
        spec.kind = kind;
        switch (kind)
        {
            case LevelQueryEntityKind::sector:
                spec.field = LevelQueryField::sectorLayer;
                break;
            case LevelQueryEntityKind::surface:
                spec.field = LevelQueryField::surfaceMaterial;
                break;
            case LevelQueryEntityKind::thing:
                spec.field = LevelQueryField::thingName;
                break;
        }
        spec.field = normalizeLevelQueryField(kind, spec.field);
        spec.matchMode = normalizeLevelQueryMatchMode(spec.field, LevelQueryMatchMode::contains);
        return spec;
    }

    LevelQuerySpec buildCurrentUiSpec(const LevelQueryEntityKind kind) const
    {
        LevelQuerySpec spec{};
        spec.kind = kind;
        spec.criteria = currentCriteria();
        if (!spec.criteria.empty())
        {
            spec.field = spec.criteria.front().field;
            spec.matchMode = spec.criteria.front().matchMode;
            spec.value = spec.criteria.front().value;
        }
        else
        {
            const LevelQuerySpec fallback = makeDefaultSpec(kind);
            spec.field = fallback.field;
            spec.matchMode = fallback.matchMode;
        }
        spec.caseSensitive = caseSensitiveCheck_->isEnabled() && caseSensitiveCheck_->isChecked();
        spec.resultMode = levelQueryResultModeFromInt(resultModeCombo_->currentData().toInt());
        spec.selectionOnly = selectionOnlyCheck_->isChecked();
        return spec;
    }

    void storeCurrentUiSpec() const
    {
        if (!kindCombo_ || !caseSensitiveCheck_ || !resultModeCombo_ || !selectionOnlyCheck_)
        {
            return;
        }

        const LevelQueryEntityKind kind = activeUiKind_;
        rememberedSpecs_[static_cast<std::size_t>(levelQueryEntityKindIndex(kind))] = buildCurrentUiSpec(kind);
    }

    void applySpecToUi(const LevelQuerySpec& spec)
    {
        activeUiKind_ = spec.kind;
        std::vector<LevelQuerySpec::Criterion> criteria = levelQueryEffectiveCriteria(spec);
        if (!forcedKind_.has_value() && criteria.empty())
        {
            criteria.push_back(LevelQuerySpec::Criterion{
                normalizeLevelQueryField(spec.kind, spec.field),
                normalizeLevelQueryMatchMode(spec.field, spec.matchMode),
                spec.value,
            });
        }

        rebuildCriteriaRows(criteria);
        caseSensitiveCheck_->setChecked(spec.caseSensitive);
        resultModeCombo_->setCurrentIndex(std::max(0, resultModeCombo_->findData(static_cast<int>(spec.resultMode))));
        selectionOnlyCheck_->setChecked(spec.selectionOnly);
        updateCaseSensitivityState();
        updateButtonState();
    }

    LevelQueryEntityKind currentKind() const noexcept
    {
        if (forcedKind_.has_value())
        {
            return *forcedKind_;
        }
        return levelQueryEntityKindFromInt(kindCombo_->currentData().toInt());
    }

    LevelQueryField rowField(const CriterionRow* row) const noexcept
    {
        if (!row)
        {
            return LevelQueryField::surfaceMaterial;
        }
        if (forcedKind_.has_value() || !row->fieldCombo)
        {
            return row->field;
        }
        return levelQueryFieldFromInt(row->fieldCombo->currentData().toInt());
    }

    void clearCriterionRows()
    {
        for (CriterionRow* row : rows_)
        {
            if (row && row->container)
            {
                criteriaLayout_->removeWidget(row->container);
                row->container->deleteLater();
            }
            delete row;
        }
        rows_.clear();
    }

    void rebuildCriteriaRows(const std::vector<LevelQuerySpec::Criterion>& preferredCriteria)
    {
        clearCriterionRows();

        if (forcedKind_.has_value())
        {
            for (const auto& option : levelQueryFieldChoices(currentKind()))
            {
                auto it = std::find_if(
                    preferredCriteria.begin(),
                    preferredCriteria.end(),
                    [&option](const LevelQuerySpec::Criterion& criterion)
                    {
                        return criterion.field == option.first;
                    });
                if (it != preferredCriteria.end())
                {
                    addCriterionRow(*it);
                }
                else
                {
                    addCriterionRow(LevelQuerySpec::Criterion{
                        option.first,
                        normalizeLevelQueryMatchMode(option.first, LevelQueryMatchMode::contains),
                        std::string{},
                    });
                }
            }
        }
        else if (preferredCriteria.empty())
        {
            addCriterionRow(std::nullopt);
        }
        else
        {
            for (const LevelQuerySpec::Criterion& criterion : preferredCriteria)
            {
                addCriterionRow(criterion);
            }
        }
        updateRemoveButtons();
        updateCaseSensitivityState();
        updateButtonState();
    }

    void addCriterionRow(const std::optional<LevelQuerySpec::Criterion>& preferred)
    {
        auto* row = new CriterionRow{};
        row->container = new QWidget(criteriaPanel_);
        auto* rowLayout = new QHBoxLayout(row->container);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        const LevelQueryField desiredField = normalizeLevelQueryField(
            currentKind(),
            preferred ? preferred->field : levelQueryFieldChoices(currentKind()).front().first);
        row->field = desiredField;

        if (forcedKind_.has_value())
        {
            QString fieldLabel = QStringLiteral("Field");
            for (const auto& option : levelQueryFieldChoices(currentKind()))
            {
                if (option.first == desiredField)
                {
                    fieldLabel = option.second;
                    break;
                }
            }
            row->fieldLabel = new QLabel(fieldLabel, row->container);
            row->fieldLabel->setMinimumWidth(118);
            row->fieldLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            rowLayout->addWidget(row->fieldLabel, 1);
        }
        else
        {
            row->fieldCombo = new QComboBox(row->container);
            for (const auto& option : levelQueryFieldChoices(currentKind()))
            {
                row->fieldCombo->addItem(option.second, static_cast<int>(option.first));
            }

            int fieldIndex = row->fieldCombo->findData(static_cast<int>(desiredField));
            if (fieldIndex < 0)
            {
                fieldIndex = 0;
            }
            row->fieldCombo->setCurrentIndex(fieldIndex);
            rowLayout->addWidget(row->fieldCombo, 1);
        }

        row->matchCombo = new QComboBox(row->container);
        rowLayout->addWidget(row->matchCombo, 1);

        row->valueEdit = new QLineEdit(row->container);
        row->valueEdit->setClearButtonEnabled(true);
        rowLayout->addWidget(row->valueEdit, 2);

        row->browseButton = new QPushButton(QStringLiteral("..."), row->container);
        row->browseButton->setFixedWidth(34);
        row->browseButton->setAutoDefault(false);
        row->browseButton->setDefault(false);
        rowLayout->addWidget(row->browseButton, 0);

        row->removeButton = new QPushButton(QStringLiteral("-"), row->container);
        row->removeButton->setFixedWidth(28);
        row->removeButton->setToolTip(QStringLiteral("Remove this criterion."));
        row->removeButton->setAutoDefault(false);
        row->removeButton->setDefault(false);
        rowLayout->addWidget(row->removeButton, 0);
        row->removeButton->setVisible(!forcedKind_.has_value());

        criteriaLayout_->addWidget(row->container);
        rows_.push_back(row);

        rebuildRowMatchOptions(row, preferred ? std::optional<LevelQueryMatchMode>(preferred->matchMode) : std::nullopt);
        row->valueEdit->setLocale(ui::uiNumericLocale());
        row->valueEdit->setText(localizeLevelQueryNumericText(
            preferred ? QString::fromStdString(preferred->value) : QString{},
            desiredField, levelQueryMatchModeFromInt(row->matchCombo->currentData().toInt())));
        updateRowPlaceholder(row);
        updateRowBrowseState(row);

        if (row->fieldCombo)
        {
            connect(row->fieldCombo, &QComboBox::currentIndexChanged, this, [this, row](int)
                {
                    rebuildRowMatchOptions(row, std::nullopt);
                    updateRowPlaceholder(row);
                    updateRowBrowseState(row);
                    updateCaseSensitivityState();
                    updateButtonState();
                });
        }
        connect(row->matchCombo, &QComboBox::currentIndexChanged, this, [this](int)
            {
                updateButtonState();
            });
        connect(row->valueEdit, &QLineEdit::textChanged, this, [this](const QString&)
            {
                updateButtonState();
            });
        connect(row->browseButton, &QPushButton::clicked, this, [this, row]()
            {
                browseRowValue(row);
            });
        connect(row->removeButton, &QPushButton::clicked, this, [this, row]()
            {
                removeCriterionRow(row);
            });

        updateRemoveButtons();
        updateCaseSensitivityState();
        updateButtonState();
    }

    void removeCriterionRow(CriterionRow* row)
    {
        if (!row)
        {
            return;
        }

        if (rows_.size() <= 1)
        {
            if (row->valueEdit)
            {
                row->valueEdit->clear();
            }
            return;
        }

        auto it = std::find(rows_.begin(), rows_.end(), row);
        if (it == rows_.end())
        {
            return;
        }

        if (row->container)
        {
            criteriaLayout_->removeWidget(row->container);
            row->container->deleteLater();
        }
        delete row;
        rows_.erase(it);
        updateRemoveButtons();
        updateCaseSensitivityState();
        updateButtonState();
    }

    void updateRemoveButtons()
    {
        if (forcedKind_.has_value())
        {
            for (CriterionRow* row : rows_)
            {
                if (row && row->removeButton)
                {
                    row->removeButton->setVisible(false);
                    row->removeButton->setEnabled(false);
                }
            }
            return;
        }

        const bool allowRemove = rows_.size() > 1;
        for (CriterionRow* row : rows_)
        {
            if (row && row->removeButton)
            {
                row->removeButton->setEnabled(allowRemove);
            }
        }
    }

    void rebuildRowMatchOptions(CriterionRow* row, const std::optional<LevelQueryMatchMode> preferred)
    {
        if (!row || !row->matchCombo)
        {
            return;
        }

        const LevelQueryField field = rowField(row);
        const LevelQueryMatchMode desired = normalizeLevelQueryMatchMode(
            field,
            preferred.value_or(levelQueryMatchModeFromInt(row->matchCombo->currentData().toInt())));

        const QSignalBlocker blocker(row->matchCombo);
        row->matchCombo->clear();
        for (const auto& option : levelQueryMatchChoices(field))
        {
            row->matchCombo->addItem(option.second, static_cast<int>(option.first));
        }

        int targetIndex = row->matchCombo->findData(static_cast<int>(desired));
        if (targetIndex < 0)
        {
            targetIndex = 0;
        }
        row->matchCombo->setCurrentIndex(targetIndex);
    }

    void updateCaseSensitivityState()
    {
        if (!caseSensitiveCheck_)
        {
            return;
        }

        bool enabled = false;
        for (const CriterionRow* row : rows_)
        {
            if (row && levelQueryFieldKind(rowField(row)) == LevelQueryFieldKind::string)
            {
                enabled = true;
                break;
            }
        }
        caseSensitiveCheck_->setEnabled(enabled);
        caseSensitiveCheck_->setToolTip(enabled
            ? QStringLiteral("Case-sensitive matching for text fields.")
            : QStringLiteral("Case sensitivity only applies to text fields."));
    }

    void updateButtonState()
    {
        bool enabled = false;
        bool valid = true;
        for (const CriterionRow* row : rows_)
        {
            if (row && row->valueEdit && !row->valueEdit->text().trimmed().isEmpty())
            {
                enabled = true;
                valid = valid && canonicalLevelQueryNumericText(
                    row->valueEdit->text().trimmed(), rowField(row),
                    levelQueryMatchModeFromInt(row->matchCombo->currentData().toInt()),
                    row->valueEdit->locale()).has_value();
            }
        }
        enabled = enabled && valid;
        if (findButton_)
        {
            findButton_->setEnabled(enabled);
        }
        if (selectAllButton_)
        {
            selectAllButton_->setEnabled(enabled);
        }
    }

    void updateRowPlaceholder(CriterionRow* row)
    {
        if (!row || !row->valueEdit)
        {
            return;
        }
        row->valueEdit->setPlaceholderText(levelQueryPlaceholder(rowField(row)));
    }

    void updateRowBrowseState(CriterionRow* row)
    {
        if (!row || !row->browseButton)
        {
            return;
        }

        const LevelQueryField field = rowField(row);
        const bool enabled =
            levelQueryPickerKind(field).has_value()
            || levelQueryUsesFlagPicker(field)
            || levelQueryUsesColorPicker(field)
            || levelQueryUsesLayerPicker(field)
            || levelQueryUsesChoicePicker(field);
        row->browseButton->setEnabled(enabled);
        row->browseButton->setToolTip(enabled
            ? QStringLiteral("Browse available values for this field.")
            : QStringLiteral("Browse is available for resource, flag, color, choice, and layer fields."));
    }

    void browseRowValue(CriterionRow* row)
    {
        if (!row || !row->valueEdit)
        {
            return;
        }

        const LevelQueryField field = rowField(row);
        if (const std::optional<ui::ResourcePickerKind> kind = levelQueryPickerKind(field))
        {
            ui::ResourcePickerOptions options{};
            options.kind = *kind;
            options.initialName = row->valueEdit->text().trimmed().toStdString();
            options.preferredDiskRoot = preferredDiskRoot_;
            options.projectDirectory = projectDirectory_;
            options.title = levelQueryBrowseTitle(field).toStdString();
            options.previewCmp = level_.getMasterCMP();
            options.previewProjectType = level_.kind;
            options.templateDatabaseFallback = templatesDb_;
            options.templateOverrides = templateOverrides_;

            const std::optional<std::string> picked = ui::pickResource(
                this, resources_, options);
            if (picked)
            {
                row->valueEdit->setText(QString::fromStdString(*picked));
            }
            return;
        }

        if (levelQueryUsesFlagPicker(field))
        {
            const std::optional<QString> picked = openLevelQueryFlagEditor(
                this,
                levelQueryBrowseTitle(field),
                row->valueEdit->text(),
                levelQueryFlagDefs(field, level_.kind));
            if (picked)
            {
                row->valueEdit->setText(*picked);
            }
            return;
        }

        if (levelQueryUsesColorPicker(field))
        {
            ColorF initialColor = levelQueryDefaultColor(field);
            LevelQuerySpec spec{};
            spec.value = canonicalLevelQueryNumericText(
                row->valueEdit->text().trimmed(), field,
                levelQueryMatchModeFromInt(row->matchCombo->currentData().toInt()),
                row->valueEdit->locale()).value_or(QString{}).toStdString();
            if (const std::optional<LevelQueryParsedColor> parsed = parseLevelQueryColor(spec))
            {
                initialColor = parsed->value;
                if (!parsed->hasAlpha && !levelQueryColorPickerShowsAlpha(field))
                {
                    initialColor.a = 1.0f;
                }
            }

            QColor current = QColor::fromRgbF(
                std::clamp(initialColor.r, 0.0f, 1.0f),
                std::clamp(initialColor.g, 0.0f, 1.0f),
                std::clamp(initialColor.b, 0.0f, 1.0f),
                std::clamp(initialColor.a, 0.0f, 1.0f));
            QColorDialog dlg(current, this);
            dlg.setWindowTitle(levelQueryBrowseTitle(field));
            dlg.setOption(QColorDialog::ShowAlphaChannel, levelQueryColorPickerShowsAlpha(field));
            if (dlg.exec() != QDialog::Accepted)
            {
                return;
            }

            const QColor chosen = dlg.selectedColor();
            const ColorF color{
                static_cast<float>(chosen.redF()),
                static_cast<float>(chosen.greenF()),
                static_cast<float>(chosen.blueF()),
                static_cast<float>(chosen.alphaF()),
            };
            row->valueEdit->setLocale(ui::uiNumericLocale());
            row->valueEdit->setText(levelQueryColorText(color, levelQueryColorPickerShowsAlpha(field)));
            row->valueEdit->setModified(false);
            return;
        }

        if (levelQueryUsesChoicePicker(field))
        {
            const auto items = levelQueryChoiceItems(level_, templatesDb_, field);
            if (items.empty())
            {
                return;
            }

            QStringList labels{};
            labels.reserve(static_cast<qsizetype>(items.size()));
            int currentIndex = 0;
            const QString currentValue = row->valueEdit->text().trimmed();
            for (int i = 0; i < static_cast<int>(items.size()); ++i)
            {
                labels.push_back(items[static_cast<std::size_t>(i)].first);
                if (items[static_cast<std::size_t>(i)].second == currentValue
                    || items[static_cast<std::size_t>(i)].first == currentValue)
                {
                    currentIndex = i;
                }
            }

            bool ok = false;
            const QString picked = QInputDialog::getItem(
                this,
                levelQueryBrowseTitle(field),
                QStringLiteral("Value"),
                labels,
                currentIndex,
                false,
                &ok);
            if (!ok || picked.isEmpty())
            {
                return;
            }

            for (const auto& item : items)
            {
                if (item.first == picked)
                {
                    row->valueEdit->setText(item.second);
                    return;
                }
            }
            return;
        }

        if (!levelQueryUsesLayerPicker(field))
        {
            return;
        }

        const QStringList layerChoices = levelQueryLayerChoices(level_);
        const qsizetype currentIndex = layerChoices.indexOf(row->valueEdit->text().trimmed());
        bool ok = false;
        const QString picked = QInputDialog::getItem(
            this,
            levelQueryBrowseTitle(field),
            QStringLiteral("Layer"),
            layerChoices,
            currentIndex < 0 ? 0 : static_cast<int>(currentIndex),
            false,
            &ok);
        if (ok && !picked.isEmpty())
        {
            row->valueEdit->setText(picked);
        }
    }

    std::vector<LevelQuerySpec::Criterion> currentCriteria() const
    {
        std::vector<LevelQuerySpec::Criterion> criteria{};
        criteria.reserve(rows_.size());
        for (const CriterionRow* row : rows_)
        {
            if (!row || !row->valueEdit || !row->matchCombo)
            {
                continue;
            }

            const QString value = row->valueEdit->text().trimmed();
            if (value.isEmpty())
            {
                continue;
            }

            criteria.push_back(LevelQuerySpec::Criterion{
                rowField(row),
                levelQueryMatchModeFromInt(row->matchCombo->currentData().toInt()),
                canonicalLevelQueryNumericText(value, rowField(row),
                    levelQueryMatchModeFromInt(row->matchCombo->currentData().toInt()),
                    row->valueEdit->locale()).value_or(value).toStdString(),
            });
        }
        return criteria;
    }

    QComboBox* kindCombo_{};
    QComboBox* resultModeCombo_{};
    QWidget* criteriaPanel_{};
    QVBoxLayout* criteriaLayout_{};
    QPushButton* addCriterionButton_{};
    QCheckBox* caseSensitiveCheck_{};
    QCheckBox* selectionOnlyCheck_{};
    QPushButton* findButton_{};
    QPushButton* selectAllButton_{};
    mutable std::array<std::optional<LevelQuerySpec>, 3> rememberedSpecs_{};
    std::shared_ptr<const assets::ResourceLocator> resources_{};
    const assets::Templates* templatesDb_{};
    const Level& level_;
    std::optional<std::filesystem::path> preferredDiskRoot_{};
    std::optional<std::filesystem::path> projectDirectory_{};
    const std::vector<ThingTemplate>* templateOverrides_{};
    std::optional<LevelQueryEntityKind> forcedKind_{};
    std::vector<CriterionRow*> rows_{};
    LevelQueryEntityKind activeUiKind_{LevelQueryEntityKind::surface};
    bool selectAll_{false};
};


} // namespace

void MainWindow::showLevelQueryDialog()
{
    auto queryKindForEditMode = [this]() noexcept
    {
        switch (editMode_)
        {
            case EditMode::sector:
                return LevelQueryEntityKind::sector;
            case EditMode::thing:
            case EditMode::frame:
                return LevelQueryEntityKind::thing;
            case EditMode::surface:
            case EditMode::light:
            case EditMode::none:
            default:
                return LevelQueryEntityKind::surface;
        }
    };

    auto pickPreferredResourceRoot = [this]() -> std::optional<std::filesystem::path>
    {
        if (!resourceRoots_.empty())
        {
            for (const std::filesystem::path& root : resourceRoots_)
            {
                if (::sed::text::iequalsAscii(root.filename().string(), "resource"))
                {
                    return root;
                }
            }
            return resourceRoots_.front();
        }
        if (!resources_.roots().empty())
        {
            for (const std::filesystem::path& root : resources_.roots())
            {
                if (::sed::text::iequalsAscii(root.filename().string(), "resource"))
                {
                    return root;
                }
            }
            return resources_.roots().front();
        }
        return std::nullopt;
    };

    auto minSelectedIndex = [](const auto& values) -> std::int32_t
    {
        if (values.empty())
        {
            return -1;
        }
        std::int32_t result = std::numeric_limits<std::int32_t>::max();
        for (const auto& value : values)
        {
            if (value < result)
            {
                result = value;
            }
        }
        return (result == std::numeric_limits<std::int32_t>::max()) ? -1 : result;
    };

    auto minSelectedSurface = [](const auto& values) -> std::optional<SurfaceRef>
    {
        std::optional<SurfaceRef> result{};
        for (const SurfaceRef& ref : values)
        {
            if (!result
                || ref.sector < result->sector
                || (ref.sector == result->sector && ref.surface < result->surface))
            {
                result = ref;
            }
        }
        return result;
    };

    auto specFromSavedState = [](const MainWindow::SavedLevelQueryState& saved, const LevelQueryEntityKind kind) -> std::optional<LevelQuerySpec>
    {
        if (!saved.hasState)
        {
            return std::nullopt;
        }

        LevelQuerySpec spec{};
        spec.kind = kind;
        spec.field = normalizeLevelQueryField(kind, levelQueryFieldFromInt(saved.field));
        spec.matchMode = levelQueryMatchModeFromInt(saved.matchMode);
        spec.caseSensitive = saved.caseSensitive;
        spec.value = saved.value;
        const std::size_t count = std::min(
            saved.criteriaFields.size(),
            std::min(saved.criteriaMatchModes.size(), saved.criteriaValues.size()));
        spec.criteria.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            spec.criteria.push_back(LevelQuerySpec::Criterion{
                normalizeLevelQueryField(kind, levelQueryFieldFromInt(saved.criteriaFields[i])),
                normalizeLevelQueryMatchMode(
                    levelQueryFieldFromInt(saved.criteriaFields[i]),
                    levelQueryMatchModeFromInt(saved.criteriaMatchModes[i])),
                saved.criteriaValues[i],
            });
        }
        spec.resultMode = levelQueryResultModeFromInt(saved.resultMode);
        spec.selectionOnly = saved.selectionOnly;
        return spec;
    };

    auto saveStateFromSpec = [](MainWindow::SavedLevelQueryState& saved, const LevelQuerySpec& spec)
    {
        saved.hasState = true;
        saved.field = static_cast<int>(normalizeLevelQueryField(spec.kind, spec.field));
        saved.matchMode = static_cast<int>(spec.matchMode);
        saved.caseSensitive = spec.caseSensitive;
        saved.value = spec.value;
        saved.criteriaFields.clear();
        saved.criteriaMatchModes.clear();
        saved.criteriaValues.clear();
        for (const LevelQuerySpec::Criterion& criterion : spec.criteria)
        {
            saved.criteriaFields.push_back(static_cast<int>(criterion.field));
            saved.criteriaMatchModes.push_back(static_cast<int>(criterion.matchMode));
            saved.criteriaValues.push_back(criterion.value);
        }
        saved.resultMode = static_cast<int>(spec.resultMode);
        saved.selectionOnly = spec.selectionOnly;
    };

    auto seededDefaultSpecForKind = [this, &minSelectedIndex, &minSelectedSurface](const LevelQueryEntityKind requestedKind) -> LevelQuerySpec
    {
        LevelQuerySpec initial{};
        bool seededFromSelection = false;

        const Level& level = document_.level();
        switch (requestedKind)
        {
            case LevelQueryEntityKind::sector:
                initial.kind = LevelQueryEntityKind::sector;
                initial.field = LevelQueryField::sectorLayer;
                {
                    std::int32_t sectorIndex = glView_ ? glView_->activeSectorIndex() : -1;
                    if (sectorIndex < 0)
                    {
                        sectorIndex = minSelectedIndex(document_.selection().sectors);
                    }
                    if (sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) < level.sectors.size())
                    {
                        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
                        initial.value = level.getLayerName(sector.layer);
                        if (initial.value.empty())
                        {
                            initial.value = std::to_string(sector.layer);
                        }
                        seededFromSelection = !initial.value.empty();
                    }
                }
                break;
            case LevelQueryEntityKind::thing:
                initial.kind = LevelQueryEntityKind::thing;
                initial.field = LevelQueryField::thingName;
                {
                    std::int32_t thingIndex = glView_ ? glView_->activeThingIndex() : -1;
                    if (thingIndex < 0)
                    {
                        thingIndex = minSelectedIndex(document_.selection().things);
                    }
                    if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) < level.things.size())
                    {
                        const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
                        if (!thing.name.empty())
                        {
                            initial.field = LevelQueryField::thingName;
                            initial.value = thing.name;
                        }
                        else
                        {
                            initial.field = LevelQueryField::thingTemplate;
                            initial.value = thing.templateName;
                        }
                        seededFromSelection = !initial.value.empty();
                    }
                }
                break;
            case LevelQueryEntityKind::surface:
                initial.kind = LevelQueryEntityKind::surface;
                initial.field = LevelQueryField::surfaceMaterial;
                {
                    std::optional<SurfaceRef> surfaceRef = glView_ ? glView_->selectedSurfaceRef() : std::nullopt;
                    if (!surfaceRef)
                    {
                        surfaceRef = minSelectedSurface(document_.selection().surfaces);
                    }
                    if (surfaceRef
                        && surfaceRef->sector >= 0
                        && surfaceRef->surface >= 0
                        && static_cast<std::size_t>(surfaceRef->sector) < level.sectors.size()
                        && static_cast<std::size_t>(surfaceRef->surface) < level.sectors[static_cast<std::size_t>(surfaceRef->sector)].surfaces.size())
                    {
                        initial.value =
                            level.sectors[static_cast<std::size_t>(surfaceRef->sector)]
                                .surfaces[static_cast<std::size_t>(surfaceRef->surface)]
                                .material;
                        seededFromSelection = !initial.value.empty();
                    }
                }
                break;
        }

        initial.field = normalizeLevelQueryField(initial.kind, initial.field);
        initial.matchMode = normalizeLevelQueryMatchMode(initial.field, initial.matchMode);
        if (seededFromSelection)
        {
            initial.matchMode = LevelQueryMatchMode::exact;
            initial.criteria = {
                LevelQuerySpec::Criterion{
                    initial.field,
                    initial.matchMode,
                    initial.value,
                }
            };
        }

        return initial;
    };

    std::array<std::optional<LevelQuerySpec>, 3> initialSpecs{};
    for (int i = 0; i < static_cast<int>(levelQueryStates_.size()); ++i)
    {
        const LevelQueryEntityKind kind = levelQueryEntityKindFromInt(i);
        if (const std::optional<LevelQuerySpec> saved = specFromSavedState(levelQueryStates_[static_cast<std::size_t>(i)], kind))
        {
            initialSpecs[static_cast<std::size_t>(i)] = saved;
        }
        else
        {
            initialSpecs[static_cast<std::size_t>(i)] = seededDefaultSpecForKind(kind);
        }
    }

    const LevelQueryEntityKind initialKind = queryKindForEditMode();
    const SavedLevelQueryState& initialSavedState =
        levelQueryStates_[static_cast<std::size_t>(levelQueryEntityKindIndex(initialKind))];
    if (!initialSavedState.hasState)
    {
        initialSpecs[static_cast<std::size_t>(levelQueryEntityKindIndex(initialKind))] = seededDefaultSpecForKind(initialKind);
    }

    const std::vector<ThingTemplate>* templateOverrides =
        !document_.level().templates.empty() ? &document_.level().templates : nullptr;
    const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;
    LevelQueryDialog dialog(
        this,
        initialSpecs,
        initialKind,
        initialKind,
        resourceLocatorSnapshot_
            ? resourceLocatorSnapshot_
            : std::make_shared<const assets::ResourceLocator>(resources_),
        templatesDb,
        document_.level(),
        pickPreferredResourceRoot(),
        currentPath_.empty()
            ? std::optional<std::filesystem::path>{}
            : std::optional<std::filesystem::path>{ currentPath_.parent_path() },
        templateOverrides);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const LevelQuerySpec spec = dialog.spec();
    levelQueryEntityKind_ = static_cast<int>(spec.kind);
    for (const auto& remembered : dialog.rememberedSpecs())
    {
        if (!remembered)
        {
            continue;
        }
        saveStateFromSpec(
            levelQueryStates_[static_cast<std::size_t>(levelQueryEntityKindIndex(remembered->kind))],
            *remembered);
    }

    std::vector<LevelQueryHit> hits = collectLevelQueryHits(document_.level(), templatesDb, spec);
    if (spec.selectionOnly)
    {
        hits = filterLevelQueryHitsToSelection(hits, document_.selection());
    }
    if (hits.empty())
    {
        const QString entityLabel = levelQueryEntityLabel(spec.kind, 2);
        const QString scopeSuffix = spec.selectionOnly ? QStringLiteral(" in current selection") : QString{};
        const std::vector<LevelQuerySpec::Criterion> criteria = levelQueryEffectiveCriteria(spec);
        if (criteria.size() <= 1)
        {
            const QString queryValue = QString::fromStdString(spec.value);
            statusBar()->showMessage(QString("No matching %1 for \"%2\"%3.").arg(entityLabel, queryValue, scopeSuffix), 5000);
            logMessage(MessageSeverity::info, QString("Find: no matching %1 for \"%2\"%3.").arg(entityLabel, queryValue, scopeSuffix));
        }
        else
        {
            statusBar()->showMessage(QString("No matching %1 for the current criteria%2.").arg(entityLabel, scopeSuffix), 5000);
            logMessage(MessageSeverity::info, QString("Find: no matching %1 for the current criteria%2.").arg(entityLabel, scopeSuffix));
        }
        return;
    }

    if (dialog.selectAllRequested())
    {
        if (!glView_)
        {
            return;
        }

        const Level& level = document_.level();
        const LevelQueryHit firstHit = hits.front();

        switch (spec.kind)
        {
            case LevelQueryEntityKind::sector:
                if (editMode_ != EditMode::sector)
                {
                    setEditMode(EditMode::sector);
                }
                break;
            case LevelQueryEntityKind::surface:
                if (editMode_ != EditMode::surface)
                {
                    setEditMode(EditMode::surface);
                }
                break;
            case LevelQueryEntityKind::thing:
                if (editMode_ != EditMode::thing)
                {
                    setEditMode(EditMode::thing);
                }
                break;
        }

        document_.selection().clear();

        QVector3D minv(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());
        QVector3D maxv(
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max());
        bool haveAny = false;

        auto include = [&](const double x, const double y, const double z)
        {
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            {
                return;
            }

            const QVector3D p(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
            if (!haveAny)
            {
                minv = p;
                maxv = p;
                haveAny = true;
                return;
            }

            minv.setX(std::min(minv.x(), p.x()));
            minv.setY(std::min(minv.y(), p.y()));
            minv.setZ(std::min(minv.z(), p.z()));
            maxv.setX(std::max(maxv.x(), p.x()));
            maxv.setY(std::max(maxv.y(), p.y()));
            maxv.setZ(std::max(maxv.z(), p.z()));
        };

        auto includeThingBounds = [&](const std::int32_t thingIndex)
        {
            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level.things.size())
            {
                return;
            }

            const Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
            if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
            {
                return;
            }

            const std::string_view tplName = !thing.templateName.empty() ? std::string_view(thing.templateName)
                                                                          : std::string_view(thing.name);

            ThingBox box = thing.bbox;
            for (const ThingTemplate& tpl : level.templates)
            {
                if (tpl.name == tplName)
                {
                    box = tpl.bbox;
                    break;
                }
            }
            if (templates_.size() > 0)
            {
                if (const ThingTemplate* tpl = templates_.find(tplName))
                {
                    box = tpl->bbox;
                }
            }

            const bool zeroBox = IsZero(box.x1) && IsZero(box.y1) && IsZero(box.z1)
                              && IsZero(box.x2) && IsZero(box.y2) && IsZero(box.z2);

            TVector localMin{};
            TVector localMax{};
            if (!zeroBox)
            {
                localMin = TVector{ box.x1, box.y1, box.z1 };
                localMax = TVector{ box.x2, box.y2, box.z2 };
            }
            else
            {
                const double halfSize = static_cast<double>(glView_->thingCubeHalfSize());
                localMin = TVector{ -halfSize, -halfSize, -halfSize };
                localMax = TVector{ halfSize, halfSize, halfSize };
            }

            TVector pyr{};
            pyr.pitch = thing.pch;
            pyr.yaw = thing.yaw;
            pyr.roll = thing.rol;
            const TMatrix43 xf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

            const TVector mn = localMin;
            const TVector mx = localMax;
            const TVector localCorners[8] = {
                TVector{ mn.x, mn.y, mn.z },
                TVector{ mx.x, mn.y, mn.z },
                TVector{ mx.x, mx.y, mn.z },
                TVector{ mn.x, mx.y, mn.z },
                TVector{ mn.x, mn.y, mx.z },
                TVector{ mx.x, mn.y, mx.z },
                TVector{ mx.x, mx.y, mx.z },
                TVector{ mn.x, mx.y, mx.z },
            };

            for (const TVector& corner : localCorners)
            {
                const TVector p = xf.TransformPoint(corner);
                include(p.x, p.y, p.z);
            }
        };

        for (const LevelQueryHit& hit : hits)
        {
            switch (hit.kind)
            {
                case OutlinerKind::sector:
                {
                    if (hit.index < 0 || static_cast<std::size_t>(hit.index) >= level.sectors.size())
                    {
                        continue;
                    }
                    document_.selection().sectors.insert(hit.index);
                    const Sector& sector = level.sectors[static_cast<std::size_t>(hit.index)];
                    for (const Vertex& v : sector.vertices)
                    {
                        include(v.x, v.y, v.z);
                    }
                    break;
                }

                case OutlinerKind::surface:
                {
                    if (hit.sector < 0 || hit.surface < 0
                        || static_cast<std::size_t>(hit.sector) >= level.sectors.size()
                        || static_cast<std::size_t>(hit.surface) >= level.sectors[static_cast<std::size_t>(hit.sector)].surfaces.size())
                    {
                        continue;
                    }

                    const SurfaceRef ref{ hit.sector, hit.surface };
                    document_.selection().surfaces.insert(ref);
                    const Sector& sector = level.sectors[static_cast<std::size_t>(hit.sector)];
                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(hit.surface)];
                    for (const std::int32_t vi : surface.vertices)
                    {
                        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                        {
                            continue;
                        }
                        const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                        include(v.x, v.y, v.z);
                    }
                    break;
                }

                case OutlinerKind::thing:
                    document_.selection().things.insert(hit.index);
                    includeThingBounds(hit.index);
                    break;

                default:
                    break;
            }
        }

        if (outlinerTabs_)
        {
            if (spec.kind == LevelQueryEntityKind::thing)
            {
                outlinerTabs_->setCurrentIndex(1);
            }
            else
            {
                outlinerTabs_->setCurrentIndex(0);
            }
        }

        std::int32_t selSector = -1;
        std::int32_t selSurface = -1;
        std::int32_t selThing = -1;
        switch (firstHit.kind)
        {
            case OutlinerKind::sector:
                selSector = firstHit.index;
                break;
            case OutlinerKind::surface:
                selSector = firstHit.sector;
                selSurface = firstHit.surface;
                break;
            case OutlinerKind::thing:
                selThing = firstHit.index;
                break;
            default:
                break;
        }

        glView_->setSelectionState(selSector, selSurface, selThing, -1, document_.selection());
        rebuildOutliner();
        updateEditActions();
        rebuildProperties();
        if (haveAny)
        {
            frameBoundsWithNavigationPrefs(minv, maxv, true);
        }

        statusBar()->showMessage(
            QString("Selected %1 matching %2.").arg(static_cast<int>(hits.size())).arg(levelQueryEntityLabel(spec.kind, static_cast<int>(hits.size()))),
            5000);
        return;
    }

    if (spec.resultMode == LevelQueryResultMode::focus)
    {
        const LevelQueryHit firstHit = hits.front();
        if (navigateToEntity(firstHit.kind, firstHit.index, firstHit.sector, firstHit.surface, true))
        {
            statusBar()->showMessage(
                QString("Found 1 of %1 matching %2.").arg(static_cast<int>(hits.size())).arg(levelQueryEntityLabel(spec.kind, static_cast<int>(hits.size()))),
                5000);
        }
        return;
    }

    if (!glView_)
    {
        return;
    }

    if (documentEditingAllowed())
    {
        switch (spec.kind)
        {
            case LevelQueryEntityKind::sector:
                if (editMode_ != EditMode::sector)
                {
                    setEditMode(EditMode::sector);
                }
                break;
            case LevelQueryEntityKind::surface:
                if (editMode_ != EditMode::surface)
                {
                    setEditMode(EditMode::surface);
                }
                break;
            case LevelQueryEntityKind::thing:
                if (editMode_ != EditMode::thing)
                {
                    setEditMode(EditMode::thing);
                }
                break;
        }
    }

    if (outlinerTabs_)
    {
        if (spec.kind == LevelQueryEntityKind::thing)
        {
            outlinerTabs_->setCurrentIndex(1);
        }
        else
        {
            outlinerTabs_->setCurrentIndex(0);
        }
    }

    editor::Selection& selection = document_.selection();
    for (const LevelQueryHit& hit : hits)
    {
        switch (hit.kind)
        {
            case OutlinerKind::sector:
                if (spec.resultMode == LevelQueryResultMode::add)
                {
                    selection.sectors.insert(hit.index);
                }
                else
                {
                    selection.sectors.erase(hit.index);
                }
                break;
            case OutlinerKind::surface:
            {
                const SurfaceRef ref{ hit.sector, hit.surface };
                if (spec.resultMode == LevelQueryResultMode::add)
                {
                    selection.surfaces.insert(ref);
                }
                else
                {
                    selection.surfaces.erase(ref);
                }
                break;
            }
            case OutlinerKind::thing:
                if (spec.resultMode == LevelQueryResultMode::add)
                {
                    selection.things.insert(hit.index);
                }
                else
                {
                    selection.things.erase(hit.index);
                }
                break;
            default:
                break;
        }
    }

    std::int32_t selSector = -1;
    std::int32_t selSurface = -1;
    std::int32_t selThing = -1;
    switch (spec.kind)
    {
        case LevelQueryEntityKind::sector:
            selSector = minSelectedIndex(selection.sectors);
            break;
        case LevelQueryEntityKind::surface:
            if (const std::optional<SurfaceRef> ref = minSelectedSurface(selection.surfaces))
            {
                selSector = ref->sector;
                selSurface = ref->surface;
            }
            break;
        case LevelQueryEntityKind::thing:
            selThing = minSelectedIndex(selection.things);
            break;
    }

    glView_->setSelectionState(selSector, selSurface, selThing, -1, selection);
    rebuildOutliner();
    updateEditActions();
    rebuildProperties();

    const QString actionLabel =
        (spec.resultMode == LevelQueryResultMode::add)
            ? QStringLiteral("Added %1 matching %2 to selection.")
            : QStringLiteral("Subtracted %1 matching %2 from selection.");
    statusBar()->showMessage(
        actionLabel.arg(static_cast<int>(hits.size())).arg(levelQueryEntityLabel(spec.kind, static_cast<int>(hits.size()))),
        5000);
}

void MainWindow::findNextLevelQuery()
{
    const LevelQueryEntityKind kind = [this]() noexcept
    {
        switch (editMode_)
        {
            case EditMode::sector:
                return LevelQueryEntityKind::sector;
            case EditMode::thing:
            case EditMode::frame:
                return LevelQueryEntityKind::thing;
            case EditMode::surface:
            case EditMode::light:
            case EditMode::none:
            default:
                return LevelQueryEntityKind::surface;
        }
    }();
    const SavedLevelQueryState& saved = levelQueryStates_[static_cast<std::size_t>(levelQueryEntityKindIndex(kind))];
    if (!saved.hasState)
    {
        showLevelQueryDialog();
        return;
    }

    LevelQuerySpec spec{};
    spec.kind = kind;
    spec.field = normalizeLevelQueryField(spec.kind, levelQueryFieldFromInt(saved.field));
    spec.matchMode = levelQueryMatchModeFromInt(saved.matchMode);
    spec.caseSensitive = saved.caseSensitive;
    spec.value = saved.value;
    {
        const std::size_t count = std::min(
            saved.criteriaFields.size(),
            std::min(saved.criteriaMatchModes.size(), saved.criteriaValues.size()));
        spec.criteria.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            spec.criteria.push_back(LevelQuerySpec::Criterion{
                normalizeLevelQueryField(
                    spec.kind,
                    levelQueryFieldFromInt(saved.criteriaFields[i])),
                normalizeLevelQueryMatchMode(
                    levelQueryFieldFromInt(saved.criteriaFields[i]),
                    levelQueryMatchModeFromInt(saved.criteriaMatchModes[i])),
                saved.criteriaValues[i],
            });
        }
    }
    spec.resultMode = levelQueryResultModeFromInt(saved.resultMode);
    spec.selectionOnly = saved.selectionOnly;

    const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;
    std::vector<LevelQueryHit> hits = collectLevelQueryHits(document_.level(), templatesDb, spec);
    if (spec.selectionOnly)
    {
        hits = filterLevelQueryHitsToSelection(hits, document_.selection());
    }
    if (hits.empty())
    {
        statusBar()->showMessage(QString("No matching %1.").arg(levelQueryEntityLabel(spec.kind, 2)), 5000);
        return;
    }

    auto minSelectedIndex = [](const auto& values) -> std::int32_t
    {
        if (values.empty())
        {
            return -1;
        }
        std::int32_t result = std::numeric_limits<std::int32_t>::max();
        for (const auto& value : values)
        {
            if (value < result)
            {
                result = value;
            }
        }
        return (result == std::numeric_limits<std::int32_t>::max()) ? -1 : result;
    };

    auto minSelectedSurface = [](const auto& values) -> std::optional<SurfaceRef>
    {
        std::optional<SurfaceRef> result{};
        for (const SurfaceRef& ref : values)
        {
            if (!result
                || ref.sector < result->sector
                || (ref.sector == result->sector && ref.surface < result->surface))
            {
                result = ref;
            }
        }
        return result;
    };

    int currentMatch = -1;
    switch (spec.kind)
    {
        case LevelQueryEntityKind::sector:
        {
            std::int32_t currentSector = glView_ ? glView_->activeSectorIndex() : -1;
            if (currentSector < 0)
            {
                currentSector = minSelectedIndex(document_.selection().sectors);
            }
            for (std::size_t i = 0; i < hits.size(); ++i)
            {
                if (hits[i].kind == OutlinerKind::sector && hits[i].index == currentSector)
                {
                    currentMatch = static_cast<int>(i);
                    break;
                }
            }
            break;
        }

        case LevelQueryEntityKind::surface:
        {
            std::optional<SurfaceRef> currentSurface = glView_ ? glView_->selectedSurfaceRef() : std::nullopt;
            if (!currentSurface)
            {
                currentSurface = minSelectedSurface(document_.selection().surfaces);
            }
            for (std::size_t i = 0; currentSurface && i < hits.size(); ++i)
            {
                if (hits[i].kind == OutlinerKind::surface
                    && hits[i].sector == currentSurface->sector
                    && hits[i].surface == currentSurface->surface)
                {
                    currentMatch = static_cast<int>(i);
                    break;
                }
            }
            break;
        }

        case LevelQueryEntityKind::thing:
        {
            std::int32_t currentThing = glView_ ? glView_->activeThingIndex() : -1;
            if (currentThing < 0)
            {
                currentThing = minSelectedIndex(document_.selection().things);
            }
            for (std::size_t i = 0; i < hits.size(); ++i)
            {
                if (hits[i].kind == OutlinerKind::thing && hits[i].index == currentThing)
                {
                    currentMatch = static_cast<int>(i);
                    break;
                }
            }
            break;
        }
    }

    int nextMatch = 0;
    if (currentMatch >= 0)
    {
        nextMatch = currentMatch + 1;
        if (nextMatch >= static_cast<int>(hits.size()))
        {
            statusBar()->showMessage(QString("No more matching %1.").arg(levelQueryEntityLabel(spec.kind, 2)), 5000);
            return;
        }
    }

    const LevelQueryHit& nextHit = hits[static_cast<std::size_t>(nextMatch)];
    if (!navigateToEntity(nextHit.kind, nextHit.index, nextHit.sector, nextHit.surface, true))
    {
        return;
    }

    QString message = QString("Found %1 of %2 matching %3.")
        .arg(nextMatch + 1)
        .arg(static_cast<int>(hits.size()))
        .arg(levelQueryEntityLabel(spec.kind, static_cast<int>(hits.size())));
    statusBar()->showMessage(message, 5000);
}


} // namespace sed
