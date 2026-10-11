#include "main_window/common.hpp"
#include "app_logging.hpp"
#include "render/selection/surface_selection_utils.hpp"
#include "render/viewport_color_theme.hpp"
#include "ui/cog/cog_script_editor_widget.hpp"
#include "ui/components/preview_worker_pool.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/resources/project_resource_settings.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCompleter>
#include <QColorDialog>
#include <QHash>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QPainter>
#include <QTimer>
#include <QWidgetAction>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QInputDialog>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStyledItemDelegate>
#include <QToolButton>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <functional>

namespace sed
{

class ThemeNameInputDialog final : public QInputDialog
{
public:
    using QInputDialog::QInputDialog;

protected:
    void showEvent(QShowEvent* event) override
    {
        QInputDialog::showEvent(event);
        // Apply the caller's initial dimensions after QInputDialog lays out
        // its compact contents. Its layout may later update its minimum size,
        // but the visible initial width remains sufficient for the prompt.
        setMinimumSize(620, 180);
        resize(std::max(width(), 620), std::max(height(), 180));
    }
};

class PreferenceShortcutEdit final : public QKeySequenceEdit
{
public:
    explicit PreferenceShortcutEdit(QWidget* const parent = nullptr)
        : QKeySequenceEdit(parent)
    {
        lineEdit_ = findChild<QLineEdit*>();
        if (lineEdit_)
        {
            QObject::connect(lineEdit_, &QLineEdit::textChanged, this,
                [this](const QString& text) { refreshDisplayText(text); });
        }
        QObject::connect(this, &QKeySequenceEdit::keySequenceChanged, this,
            [this](const QKeySequence&) { refreshDisplayText(lineEdit_ ? lineEdit_->text() : QString{}); });
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ShortcutOverride)
        {
            event->accept();
            return true;
        }
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            const auto* keyEvent = static_cast<QKeyEvent*>(event);
            const Qt::KeyboardModifier modifier =
                ui::shortcuts::modifierForEditorKey(keyEvent->key());
            if (modifier != Qt::NoModifier && !keyEvent->isAutoRepeat())
            {
                if (event->type() == QEvent::KeyPress)
                {
                    modifierChord_ |= ui::shortcuts::modifiersWithKeyState(
                        keyEvent->modifiers(), keyEvent->key(), true);
                    modifierKey_ = keyEvent->key();
                }
                else if (modifierChord_ != Qt::NoModifier)
                {
                    // QKeySequenceEdit normally discards modifier-only input.
                    // Record the complete hold chord when its keys are released.
                    setKeySequence(QKeySequence(int(modifierChord_
                        & ~ui::shortcuts::modifierForEditorKey(modifierKey_)) | modifierKey_));
                    if (ui::shortcuts::modifiersWithKeyState(
                        keyEvent->modifiers(), keyEvent->key(), false) == Qt::NoModifier)
                    {
                        modifierChord_ = Qt::NoModifier;
                    }
                }
                event->accept();
                return true;
            }
            if (event->type() == QEvent::KeyPress)
            {
                modifierChord_ = Qt::NoModifier;
            }
        }
        if (event->type() == QEvent::FocusOut)
        {
            modifierChord_ = Qt::NoModifier;
        }
        return QKeySequenceEdit::event(event);
    }

private:
    void refreshDisplayText(const QString& nativeText)
    {
        if (!lineEdit_ || refreshingDisplay_)
        {
            return;
        }

        QKeySequence sequenceForDisplay = keySequence();
        const QString nativeSequence = sequenceForDisplay.toString(QKeySequence::NativeText);
        QString unfinishedSuffix{};
        if (nativeText != nativeSequence)
        {
            // QKeySequenceEdit updates its line edit as keys arrive and may
            // show the next-chord suffix before publishing keySequence().
            // Parse the visible native buffer so recording stays readable
            // during that interval, while leaving partial/unparseable input
            // to Qt unchanged.
            QString parseText = nativeText;
            const QString pendingSuffix = QStringLiteral(", ...");
            if (parseText.endsWith(pendingSuffix))
            {
                parseText.chop(pendingSuffix.size());
                unfinishedSuffix = pendingSuffix;
            }
            const QKeySequence parsed(parseText, QKeySequence::NativeText);
            if (parsed.isEmpty()
                || parsed.toString(QKeySequence::NativeText) != parseText)
            {
                return;
            }
            sequenceForDisplay = parsed;
        }

        const QString displayText = ui::shortcuts::keySequenceDisplayText(sequenceForDisplay)
            + unfinishedSuffix;
        if (displayText == nativeText)
        {
            return;
        }

        refreshingDisplay_ = true;
        const int cursorPosition = lineEdit_->cursorPosition();
        const QSignalBlocker blocker(lineEdit_);
        lineEdit_->setText(displayText);
        lineEdit_->setCursorPosition(std::min(
            cursorPosition, static_cast<int>(displayText.size())));
        refreshingDisplay_ = false;
    }

    QLineEdit* lineEdit_{};
    bool refreshingDisplay_{};
    Qt::KeyboardModifiers modifierChord_{};
    int modifierKey_{};
};

class PreferenceShortcutAlternatives final : public QWidget
{
public:
    PreferenceShortcutAlternatives(
        const QList<QKeySequence>& shortcuts, const bool modifierOnly,
        const QString& helpText, QWidget* const parent)
        : QWidget(parent), modifierOnly_(modifierOnly)
    {
        auto* const layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        editor_ = new PreferenceShortcutEdit(this);
        editor_->setObjectName("pref_keymap_shortcut_edit");
        editor_->setProperty("sed_modifier_only_shortcut", modifierOnly);
        editor_->setMinimumHeight(std::max(26, editor_->fontMetrics().height() + 10));
        editor_->setToolTip(modifierOnly
            ? QStringLiteral("%1\nHold bindings accept modifier keys only (Ctrl, Shift, Alt, or Meta); clear the field to disable.").arg(helpText)
            : helpText);
        selector_ = new QComboBox(this);
        selector_->setObjectName("pref_keymap_alternative");
        selector_->setProperty("sed.keymapAlternativeSelector", true);
        selector_->setToolTip("Choose the shortcut alternative to edit.");
        selector_->setMinimumWidth(65);
        selector_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        add_ = new QToolButton(this);
        add_->setObjectName("pref_keymap_add");
        add_->setText("+");
        add_->setToolTip("Add a shortcut alternative.");
        add_->setAccessibleName("Add shortcut alternative");
        remove_ = new QToolButton(this);
        remove_->setObjectName("pref_keymap_remove");
        remove_->setText(QString(QChar(0x2212)));
        remove_->setToolTip("Remove the selected shortcut alternative. Removing the last shortcut disables the action.");
        remove_->setAccessibleName("Remove shortcut alternative");
        for ( QToolButton* const button : { add_, remove_ } )
        {
            button->setAutoRaise(true);
            button->setFixedSize(24, 24);
        }
        layout->addWidget(editor_, 1);
        layout->addWidget(selector_);
        layout->addWidget(add_);
        layout->addWidget(remove_);
        setShortcuts(shortcuts);
        QObject::connect(selector_, &QComboBox::currentIndexChanged, this, [this](const int index)
        {
            if ( index >= 0 && index < sequences_.size() )
            {
                const QSignalBlocker blocker(editor_);
                editor_->setKeySequence(sequences_[index]);
            }
        });
        QObject::connect(editor_, &QKeySequenceEdit::keySequenceChanged, this, [this](QKeySequence sequence)
        {
            if ( modifierOnly_ && !sequence.isEmpty()
                && !ui::shortcuts::decodeModifierOnlyShortcut(sequence).has_value() )
            {
                sequence = QKeySequence{};
                const QSignalBlocker blocker(editor_);
                editor_->setKeySequence(sequence);
                QToolTip::showText(editor_->mapToGlobal(QPoint(0, editor_->height())),
                    "This hold binding accepts only Ctrl, Shift, Alt, Meta, or their combinations.", editor_);
            }
            const int index = selector_->currentIndex();
            if ( index >= 0 && index < sequences_.size() )
            {
                sequences_[index] = sequence;
                remove_->setEnabled(sequences_.size() > 1 || !sequence.isEmpty());
                notifyChanged();
            }
        });
        QObject::connect(add_, &QToolButton::clicked, this, [this]()
        {
            sequences_.push_back(QKeySequence{});
            refreshSelector(static_cast<int>(sequences_.size() - 1));
            editor_->setFocus(Qt::OtherFocusReason);
            notifyChanged();
        });
        QObject::connect(remove_, &QToolButton::clicked, this, [this]()
        {
            const int index = selector_->currentIndex();
            if ( index < 0 )
            {
                return;
            }
            if ( sequences_.size() > 1 )
            {
                sequences_.removeAt(index);
            }
            else
            {
                sequences_[0] = QKeySequence{};
            }
            refreshSelector(std::min(index, static_cast<int>(sequences_.size() - 1)));
            notifyChanged();
        });
    }

    QList<QKeySequence> shortcuts() const
    {
        QList<QKeySequence> result{};
        for ( const QKeySequence& sequence : sequences_ )
        {
            if ( !sequence.isEmpty() && !result.contains(sequence) )
            {
                result.push_back(sequence);
            }
        }
        return result;
    }

    void setShortcuts(const QList<QKeySequence>& shortcuts)
    {
        sequences_.clear();
        for ( const QKeySequence& sequence : shortcuts )
        {
            if ( !sequence.isEmpty() && !sequences_.contains(sequence)
                && (!modifierOnly_ || ui::shortcuts::decodeModifierOnlyShortcut(sequence).has_value()) )
            {
                sequences_.push_back(sequence);
            }
        }
        if ( sequences_.isEmpty() )
        {
            sequences_.push_back(QKeySequence{});
        }
        refreshSelector(0);
        notifyChanged();
    }

    void setChangeHandler(std::function<void()> changed)
    {
        changed_ = std::move(changed);
    }

private:
    void refreshSelector(const int selected)
    {
        const QSignalBlocker selectorBlocker(selector_);
        const QSignalBlocker editorBlocker(editor_);
        selector_->clear();
        for ( qsizetype index = 0; index < sequences_.size(); ++index )
        {
            selector_->addItem(QStringLiteral("%1 / %2").arg(index + 1).arg(sequences_.size()));
        }
        selector_->setCurrentIndex(selected);
        editor_->setKeySequence(sequences_[selected]);
        selector_->setEnabled(sequences_.size() > 1);
        remove_->setEnabled(sequences_.size() > 1 || !sequences_[selected].isEmpty());
    }

    void notifyChanged()
    {
        if ( changed_ && !signalsBlocked() )
        {
            changed_();
        }
    }

    PreferenceShortcutEdit* editor_{};
    QComboBox* selector_{};
    QToolButton* add_{};
    QToolButton* remove_{};
    QList<QKeySequence> sequences_{};
    bool modifierOnly_{};
    std::function<void()> changed_{};
};

constexpr int kCameraKeymapLinkHighlightRole = Qt::UserRole + 42;

class PreferenceKeymapDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* const painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        QStyleOptionViewItem clean = option;
        clean.state &= ~QStyle::State_MouseOver;
        QStyledItemDelegate::paint(painter, clean, index);
        if ( index.data(kCameraKeymapLinkHighlightRole).toBool() )
        {
            painter->save();
            painter->fillRect(option.rect, QColor(55, 130, 212, 98));
            painter->restore();
        }
    }
};

class PreferenceKeymapTree final : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

protected:
    void paintEvent(QPaintEvent* const event) override
    {
        // QTreeView rebuilds row hover flags from its cached hover index after
        // drawRow begins. Clear that cache through its item-view hover-leave path
        // before painting, preserving actual mouse events, selection and tips.
        executeDelayedItemsLayout();
        QEvent clearHover(QEvent::HoverLeave);
        QAbstractItemView::viewportEvent(&clearHover);
        QTreeWidget::paintEvent(event);
    }
};

class PreferenceKeymapItem final : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
        const int comparison = QString::compare(text(column), other.text(column), Qt::CaseInsensitive);
        return comparison != 0 ? comparison < 0
            : QString::compare(text(column), other.text(column), Qt::CaseSensitive) < 0;
    }
};

#include "main_window/detail/helpers.hpp"

void MainWindow::showPreferencesDialog(QWidget* parent)
{
    const float originalSurfaceNormalLength = glView_
        ? glView_->surfaceNormalLengthScale() : 1.0f;
    const float originalSurfaceNormalArrowhead = glView_
        ? glView_->surfaceNormalArrowheadScale() : 1.0f;
    const bool originalShowSurfaceNormalArrowhead = glView_
        ? glView_->showSurfaceNormalArrowhead() : true;
    bool cogFontPreviewChanged = false;
    QDialog dialog(parent ? parent : this);
    dialog.setLocale(ui::uiNumericLocale());
    dialog.setWindowTitle("Preferences");
    dialog.setModal(true);
    dialog.resize(1140, 720);
    dialog.setMinimumSize(1040, 640);
    {
        QFont f = dialog.font();
        f.setPointSize(std::max(7, f.pointSize() - 2));
        dialog.setFont(f);
    }
    dialog.setStyleSheet(QStringLiteral(R"(
QWidget#pref_section_root
{
    background-color: #2f2f2f;
    border: 1px solid #202020;
    border-radius: 6px;
}

 QListWidget#pref_categories
 {
     background-color: #343434;
     border: 1px solid #202020;
     border-radius: 6px;
     padding: 4px;
     outline: 0;
 }

 QListWidget#pref_categories::item
 {
     padding: 6px 8px;
     border-radius: 6px;
 }

QListWidget#pref_categories::item:selected
{
    background-color: rgba(150, 156, 164, 104);
    color: #ffffff;
}

QListWidget#pref_categories::item:focus,
QListWidget#pref_categories::item:selected:focus
{
    outline: none;
}

QToolButton#pref_section_header
{
    background-color: #3a3a3a;
    border: none;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    padding: 6px 10px;
    font-weight: 600;
}

QFrame#pref_section_content
{
    background: transparent;
    border: none;
}

QLineEdit#pref_path_edit
{
    background-color: #1f1f1f;
    border: 1px solid #202020;
    border-radius: 4px;
    border-top-right-radius: 0px;
    border-bottom-right-radius: 0px;
    padding: 0px 8px;
    min-height: 20px;
}

QLineEdit#pref_path_edit:hover
{
    border-color: #2b2b2b;
}

QLineEdit#pref_path_edit:focus
{
    border-color: #2f6db1;
}

QToolButton#pref_path_browse
{
    background-color: #1f1f1f;
    border: 1px solid #202020;
    border-left: none;
    border-radius: 4px;
    border-top-left-radius: 0px;
    border-bottom-left-radius: 0px;
    padding: 0px;
    margin: 0px;
    min-height: 20px;
    min-width: 20px;
}

QToolButton#pref_path_browse:hover
{
    background-color: #232323;
}

QToolButton#pref_path_browse:pressed
{
    background-color: #1a1a1a;
}

QComboBox
{
    background-color: #1f1f1f;
    border: 1px solid #202020;
    border-radius: 8px;
    padding: 3px 10px;
    min-height: 22px;
    color: palette(text);
}

QComboBox::drop-down
{
    border: none;
    width: 26px;
}

QComboBox::down-arrow
{
    width: 10px;
    height: 10px;
}

QComboBox QAbstractItemView
{
    background-color: #2f2f2f;
    border: 1px solid #202020;
    selection-background-color: rgba(150, 156, 164, 104);
}

QTreeWidget#pref_keymap_tree
{
    background-color: #2b2b2b;
    alternate-background-color: #303030;
    border: 1px solid #202020;
    border-radius: 6px;
}

QTreeWidget#pref_keymap_tree::item
{
    /* Keep rows clean; category/alternate background shades provide the
       grouping cue without drawing a separator line through every row. */
    border: none;
    padding: 2px 3px;
}

QKeySequenceEdit#pref_keymap_shortcut_edit
{
    margin: 1px 2px;
    padding: 0px;
}

QTreeWidget#pref_keymap_tree::item:selected
{
    background-color: rgba(150, 156, 164, 104);
    color: #ffffff;
}

QTreeWidget#pref_keymap_tree::item:selected:active
{
    background-color: rgba(150, 156, 164, 104);
    color: #ffffff;
}

QTreeWidget#pref_keymap_tree::item:selected:!active
{
    background-color: rgba(150, 156, 164, 72);
    color: #ffffff;
}
)") + ui::modernCheckBoxStyleSheet());

    auto* layout = new QVBoxLayout(&dialog);

    struct ProjectRow
    {
        ProjectType kind{};
        QLineEdit* edit{};
        std::optional<std::filesystem::path> originalInstallDir{};
    };

    using ExportBufferMember = std::uint32_t LevelExportBufferExtras::*;
    struct ExportBufferField
    {
        const char* label{};
        const char* objectSuffix{};
        ExportBufferMember member{};
    };
    constexpr std::array<ExportBufferField, 12> exportBufferFields{
        ExportBufferField{ "Sounds", "sounds", &LevelExportBufferExtras::sounds },
        ExportBufferField{ "Materials", "materials", &LevelExportBufferExtras::materials },
        ExportBufferField{ "AI classes", "ai_classes", &LevelExportBufferExtras::aiClasses },
        ExportBufferField{ "Models / 3DO", "models", &LevelExportBufferExtras::models },
        ExportBufferField{ "Sprites", "sprites", &LevelExportBufferExtras::sprites },
        ExportBufferField{ "Keyframes", "keyframes", &LevelExportBufferExtras::keyframes },
        ExportBufferField{ "Puppets", "puppets", &LevelExportBufferExtras::puppets },
        ExportBufferField{ "Sound classes", "sound_classes", &LevelExportBufferExtras::soundClasses },
        ExportBufferField{ "COG scripts", "cog_scripts", &LevelExportBufferExtras::cogScripts },
        ExportBufferField{ "COG instances", "cogs", &LevelExportBufferExtras::cogs },
        ExportBufferField{ "Templates", "templates", &LevelExportBufferExtras::templates },
        ExportBufferField{ "Things", "things", &LevelExportBufferExtras::things },
    };
    constexpr std::array<ProjectType, 3> exportBufferProjects{
        ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM };
    std::array<std::array<QSpinBox*, exportBufferFields.size()>,
        exportBufferProjects.size()> exportBufferSpins{};

    std::vector<ProjectRow> rows{};
    rows.reserve(3);

    struct KeymapEditRow
    {
        QString actionId{};
        QTreeWidgetItem* item{};
        PreferenceShortcutAlternatives* controls{};
        QString searchText{};
    };

    std::vector<KeymapEditRow> keymapRows{};
    std::unordered_map<std::string, QAction*> keymapActionsById{};
    bool mouseNavigationValid = true;
    bool keymapValid = true;
    const auto updatePreferenceAcceptState = [&]()
    {
        if ( auto* const buttonBox = dialog.findChild<QDialogButtonBox*>() )
        {
            if ( auto* const ok = buttonBox->button(QDialogButtonBox::Ok) )
            {
                ok->setEnabled(mouseNavigationValid && keymapValid);
            }
        }
    };

    auto* content = new QWidget(&dialog);
    auto* contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);
    layout->addWidget(content, 1);

    auto* categories = new QListWidget(content);
    categories->setObjectName("pref_categories");
    categories->setFixedWidth(140);
    categories->setFrameShape(QFrame::NoFrame);
    categories->setSelectionMode(QAbstractItemView::SingleSelection);
    categories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    categories->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    categories->setWordWrap(true);
    contentLayout->addWidget(categories);

    auto* stack = new QStackedWidget(content);
    contentLayout->addWidget(stack, 1);

    auto* resourcesPage = new QWidget(&dialog);
    resourcesPage->setProperty(
        "sed.preferencesDisclosurePath", QStringLiteral("resources"));
    auto* resourcesLayout = new QVBoxLayout(resourcesPage);
    resourcesLayout->setContentsMargins(0, 0, 0, 0);
    resourcesLayout->setSpacing(12);

    // Disclosure choices are workspace UI state, not user configuration.
    // Retain them only for this application process and never write them to
    // QSettings. Page/path-qualified keys remain stable if section titles are
    // reused on different Preferences pages later.
    static QHash<QString, bool> sessionDisclosureState{};
    auto createCollapsedSection = [](const QString& stableId,
                                      const QString& title,
                                      QWidget* parent,
                                      const bool defaultExpanded = false)
    {
        auto* section = new ui::CollapsibleSection(title, parent);
        const QString parentPath = parent
            ? parent->property("sed.preferencesDisclosurePath").toString()
            : QString{};
        const QString path = parentPath.isEmpty()
            ? QStringLiteral("preferences/%1").arg(stableId)
            : QStringLiteral("%1/%2").arg(parentPath, stableId);
        section->setProperty("sed.preferencesDisclosurePath", path);
        section->setExpandedImmediate(
            sessionDisclosureState.value(path, defaultExpanded));
        if (auto* header = section->findChild<QToolButton*>(
                QStringLiteral("pref_section_header"),
                Qt::FindDirectChildrenOnly))
        {
            QObject::connect(header, &QToolButton::toggled, section,
                [path](const bool expanded)
                {
                    sessionDisclosureState.insert(path, expanded);
                });
        }
        return section;
    };

    const auto addProjectBadge = [](QWidget* parent,
                                    const QString& objectName,
                                    const QString& text,
                                    const QString& accessibleName)
    {
        auto* badge = new QLabel(text, parent);
        badge->setObjectName(objectName);
        badge->setProperty("sed.projectApplicabilityBadge", true);
        badge->setAccessibleName(accessibleName);
        badge->setAlignment(Qt::AlignCenter);
        badge->setContentsMargins(7, 2, 7, 2);
        badge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        badge->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  color: #d7e7ff;"
            "  background-color: rgba(67, 111, 164, 92);"
            "  border: 1px solid rgba(133, 174, 224, 100);"
            "  border-radius: 7px;"
            "  font-size: 9pt;"
            "  font-weight: 600;"
            "}"));
        return badge;
    };

    // ---- General preferences page ----
    // Application-wide authoring behavior belongs here rather than under
    // viewport presentation. Keep Thing-specific choices grouped so the page
    // can grow without turning into an unstructured list.
    auto* generalPage = new QWidget(&dialog);
    generalPage->setProperty(
        "sed.preferencesDisclosurePath", QStringLiteral("general"));
    auto* generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setContentsMargins(0, 0, 0, 0);
    generalLayout->setSpacing(12);

    auto* numbersSection = createCollapsedSection(
        QStringLiteral("numbers"), "Numbers", generalPage, true);
    auto* numbersForm = new QFormLayout();
    numbersForm->setContentsMargins(6, 4, 6, 4);
    numbersForm->setSpacing(6);
    auto* decimalSeparator = new QComboBox(numbersSection);
    decimalSeparator->setObjectName("pref_decimal_separator");
    decimalSeparator->addItem("Dot (.)", static_cast<int>(UiDecimalSeparator::dot));
    decimalSeparator->addItem("Comma (,)", static_cast<int>(UiDecimalSeparator::comma));
    decimalSeparator->addItem("System locale", static_cast<int>(UiDecimalSeparator::system));
    const UiDecimalSeparator originalDecimalSeparator = loadUiDecimalSeparator();
    decimalSeparator->setCurrentIndex(decimalSeparator->findData(
        static_cast<int>(originalDecimalSeparator)));
    decimalSeparator->setToolTip(
        "Choose the decimal separator used to display and enter numbers. "
        "Digit grouping is disabled. Changes apply when you click OK.");
    numbersForm->addRow("Decimal separator", decimalSeparator);
    numbersSection->contentLayout()->addLayout(numbersForm);
    generalLayout->addWidget(numbersSection);

    auto* generalThingSection = createCollapsedSection(
        QStringLiteral("things"), "Things", generalPage, true);
    {
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        auto* templatePickOverridesName = new QCheckBox(
            "Use the picked template name as the Thing name",
            generalThingSection);
        templatePickOverridesName->setObjectName(
            "pref_thing_template_pick_overrides_name");
        templatePickOverridesName->setChecked(
            settingsStore().value(
                "things/template_pick_overrides_name", false).toBool());
        templatePickOverridesName->setToolTip(
            "When you pick a template for an existing Thing, also replace "
            "its name with the template name. This is off by default so "
            "designer-assigned Thing names are preserved.");
        form->addRow("Template picker", templatePickOverridesName);

        generalThingSection->contentLayout()->addLayout(form);
    }
    generalLayout->addWidget(generalThingSection);

    auto* resourcePreviewSection = createCollapsedSection(
        QStringLiteral("resource-previews"),
        "Resource previews", generalPage);
    {
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        auto* workerCount = new QSpinBox(resourcePreviewSection);
        workerCount->setObjectName(
            "pref_resource_preview_worker_count");
        workerCount->setRange(
            ui::detail::kMinPreviewWorkerCount,
            ui::detail::kMaxPreviewWorkerCount);
        workerCount->setValue(settingsStore().value(
            "resource_previews/worker_count",
            ui::detail::defaultPreviewWorkerCount()).toInt());
        workerCount->setToolTip(
            "Number of CPU worker threads used to prepare model and sprite "
            "thumbnails. More workers can fill lists faster, but use more "
            "CPU and may make the editor less responsive.");
        form->addRow("Worker threads", workerCount);

        auto* publishBatch = new QSpinBox(resourcePreviewSection);
        publishBatch->setObjectName(
            "pref_resource_preview_publish_batch_count");
        publishBatch->setRange(
            ui::detail::kMinPreviewPublishBatchCount,
            ui::detail::kMaxPreviewPublishBatchCount);
        publishBatch->setValue(settingsStore().value(
            "resource_previews/publish_batch_count",
            ui::detail::kDefaultPreviewPublishBatchCount).toInt());
        publishBatch->setToolTip(
            "Maximum thumbnail icons published during each interface "
            "update. Larger batches fill visible rows faster, but can make "
            "scrolling, preview animation, and typing less responsive.");
        form->addRow("Publish per update", publishBatch);

        resourcePreviewSection->contentLayout()->addLayout(form);
    }
    generalLayout->addWidget(resourcePreviewSection);

    auto* loggingSection = createCollapsedSection(
        QStringLiteral("logging"), "Logging", generalPage);
    {
        const ApplicationLogSettings logSettings =
            loadApplicationLogSettings();
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        auto* enabled = new QCheckBox("Enable file logging",
            loggingSection);
        enabled->setObjectName("pref_logging_enabled");
        enabled->setChecked(logSettings.enabled);
        enabled->setToolTip(
            "Master switch for all file logging. Turning this off stops every "
            "category and severity from being written to sed.log without "
            "changing the category selections below.");
        form->addRow("Master switch", enabled);

        auto* minimumLevel = new QComboBox(loggingSection);
        minimumLevel->setObjectName("pref_logging_minimum_level");
        minimumLevel->addItem("Debug details",
            static_cast<int>(ApplicationLogLevel::debug));
        minimumLevel->addItem("Information",
            static_cast<int>(ApplicationLogLevel::info));
        minimumLevel->addItem("Warnings",
            static_cast<int>(ApplicationLogLevel::warning));
        minimumLevel->addItem("Errors only",
            static_cast<int>(ApplicationLogLevel::error));
        minimumLevel->setCurrentIndex(std::max(0,
            minimumLevel->findData(static_cast<int>(
                logSettings.minimumLevel))));
        minimumLevel->setToolTip(
            "Debug details include reasons why editing tools reject an "
            "operation. Warnings is the recommended normal setting.");
        form->addRow("Minimum detail", minimumLevel);

        const auto addCategory = [form, loggingSection](
            const char* objectName,
            const char* label,
            const bool checked,
            const char* tooltip) -> QCheckBox*
        {
            auto* check = new QCheckBox(label, loggingSection);
            check->setObjectName(QString::fromLatin1(objectName));
            check->setChecked(checked);
            check->setToolTip(tooltip);
            form->addRow(QString{}, check);
            return check;
        };
        auto* applicationCategory = addCategory(
            "pref_logging_category_application", "Application",
            logSettings.applicationEnabled,
            "Editor workflows and uncategorized important failures.");
        auto* guiCategory = addCategory(
            "pref_logging_category_gui", "GUI",
            logSettings.guiEnabled,
            "Windows, widgets, dialogs, and Qt interface diagnostics.");
        auto* coreCategory = addCategory(
            "pref_logging_category_core", "Core",
            logSettings.coreEnabled,
            "Portable level operations and deterministic core diagnostics.");
        auto* resourcesCategory = addCategory(
            "pref_logging_category_resources", "Resources",
            logSettings.resourcesEnabled,
            "Resource lookup, parsing, preview, texture, model, sprite, and "
            "sound diagnostics.");

        auto* maximumFileSize = new QSpinBox(loggingSection);
        maximumFileSize->setObjectName("pref_logging_maximum_file_mib");
        maximumFileSize->setRange(1, 100);
        maximumFileSize->setSuffix(" MiB");
        maximumFileSize->setValue(static_cast<int>(
            logSettings.maximumFileMiB));
        maximumFileSize->setToolTip(
            "When sed.log reaches this size, the previous log is retained "
            "as sed.log.1 and a new log is started.");
        form->addRow("Maximum size", maximumFileSize);

        auto* logPath = new QLineEdit(loggingSection);
        logPath->setObjectName("pref_logging_path");
        logPath->setReadOnly(true);
        logPath->setText(QDir::toNativeSeparators(
            QString::fromStdWString(applicationLogPath().wstring())));
        logPath->setToolTip(logPath->text());
        form->addRow("Log file", logPath);

        const auto setDependentControlsEnabled =
            [minimumLevel, maximumFileSize, applicationCategory,
             guiCategory, coreCategory, resourcesCategory](
                const bool loggingEnabled)
        {
            minimumLevel->setEnabled(loggingEnabled);
            maximumFileSize->setEnabled(loggingEnabled);
            applicationCategory->setEnabled(loggingEnabled);
            guiCategory->setEnabled(loggingEnabled);
            coreCategory->setEnabled(loggingEnabled);
            resourcesCategory->setEnabled(loggingEnabled);
        };
        connect(enabled, &QCheckBox::toggled, loggingSection,
            setDependentControlsEnabled);
        setDependentControlsEnabled(enabled->isChecked());

        loggingSection->contentLayout()->addLayout(form);
    }
    generalLayout->addWidget(loggingSection);
    generalLayout->addStretch(1);

    auto* generalScroll = new QScrollArea(&dialog);
    generalScroll->setWidgetResizable(true);
    generalScroll->setFrameShape(QFrame::NoFrame);
    generalScroll->setWidget(generalPage);

    categories->addItem("General");
    stack->addWidget(generalScroll);

    auto addProjectRow = [&](const ProjectType kind, const QString& fullName, const QString& shortName)
    {
        const QString stableId = kind == ProjectType::IJIM
            ? QStringLiteral("ijim-install")
            : (kind == ProjectType::MOTS
                    ? QStringLiteral("mots-install")
                    : QStringLiteral("jkdf2-install"));
        auto* section = createCollapsedSection(
            stableId, fullName, resourcesPage);

        auto* rowWidget = new QWidget(section);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(3);

        auto* edit = new QLineEdit(rowWidget);
        edit->setObjectName("pref_path_edit");
        edit->setProperty(
            "sed.projectKind", static_cast<int>(kind));
        edit->setPlaceholderText("Path to game install or Resource directory");
        const std::optional<std::filesystem::path> current =
            readProjectInstallDirSetting(kind);
        if (current)
        {
            edit->setText(QString::fromStdWString(current->wstring()));
        }

        auto* browse = new QToolButton(rowWidget);
        browse->setObjectName("pref_path_browse");
        browse->setAutoRaise(true);
        browse->setCursor(Qt::PointingHandCursor);
        const int editHeight = edit->sizeHint().height();
        const int iconSize = std::max(8, editHeight - 4);
        browse->setIcon(makeFlatFolderIcon(QColor(210, 210, 210), iconSize, browse->devicePixelRatioF()));
        browse->setIconSize(QSize(iconSize, iconSize));
        browse->setToolTip(QString("Browse for %1 directory").arg(shortName));
        browse->setFixedWidth(editHeight);
        browse->setFixedHeight(editHeight);

        QObject::connect(browse, &QToolButton::clicked, &dialog, [edit, &dialog, shortName]()
            {
                QString startDir = edit->text().trimmed();
                if (startDir.isEmpty())
                {
                    startDir = QDir::homePath();
                }

                const QString chosen =
                    QFileDialog::getExistingDirectory(&dialog, QString("Select %1 directory").arg(shortName), startDir);
                if (!chosen.isEmpty())
                {
                    edit->setText(chosen);
                }
            });

        rowLayout->addWidget(edit, 1);
        rowLayout->addWidget(browse);

        section->contentLayout()->addWidget(rowWidget);
        resourcesLayout->addWidget(section);

        rows.push_back(ProjectRow{ kind, edit, current });
    };

    addProjectRow(ProjectType::IJIM, "Indiana Jones and the Infernal Machine", "Indiana Jones and the Infernal Machine");
    addProjectRow(ProjectType::JKDF2, "Star Wars Jedi Knight: Dark Forces II", "Star Wars Jedi Knight: Dark Forces II");
    addProjectRow(ProjectType::MOTS, "Star Wars Jedi Knight: Mysteries of the Sith", "Star Wars Jedi Knight: Mysteries of the Sith");

    auto* exportBufferSection = createCollapsedSection(
        QStringLiteral("new-project-extra-buffers"),
        "New Project Extra Buffers", resourcesPage);
    auto* exportBufferExplanation = new QLabel(
        "These project-specific values initialize new levels. Existing or imported levels keep their own editable Extra values.",
        exportBufferSection);
    exportBufferExplanation->setWordWrap(true);
    exportBufferExplanation->setStyleSheet(
        "color: rgba(220, 220, 220, 180);");
    exportBufferSection->contentLayout()->addWidget(exportBufferExplanation);

    auto* exportBufferTable = new QTableWidget(
        static_cast<int>(exportBufferFields.size()),
        static_cast<int>(exportBufferProjects.size()) + 1,
        exportBufferSection);
    exportBufferTable->setObjectName("pref_new_project_export_buffers");
    exportBufferTable->setHorizontalHeaderLabels(
        { "Resource", "JKDF2", "MotS", "IJIM" });
    exportBufferTable->verticalHeader()->hide();
    exportBufferTable->setAlternatingRowColors(true);
    exportBufferTable->setSelectionMode(QAbstractItemView::NoSelection);
    exportBufferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    exportBufferTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    for (int column = 1; column < exportBufferTable->columnCount(); ++column)
    {
        exportBufferTable->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }

    const QSettings exportBufferSettings = settingsStore();
    const auto projectObjectName = [](const ProjectType kind)
    {
        switch (kind)
        {
            case ProjectType::JKDF2: return QStringLiteral("jkdf2");
            case ProjectType::MOTS: return QStringLiteral("mots");
            case ProjectType::IJIM: return QStringLiteral("ijim");
        }
        return QStringLiteral("unknown");
    };
    for (std::size_t row = 0; row < exportBufferFields.size(); ++row)
    {
        auto* label = new QTableWidgetItem(
            QString::fromLatin1(exportBufferFields[row].label));
        label->setFlags(Qt::ItemIsEnabled);
        exportBufferTable->setItem(static_cast<int>(row), 0, label);

        for (std::size_t project = 0;
             project < exportBufferProjects.size(); ++project)
        {
            const ProjectType kind = exportBufferProjects[project];
            const LevelExportBufferExtras configured =
                ui::project::configuredNewProjectExportBufferExtras(
                    exportBufferSettings, kind);
            auto* spin = new QSpinBox(exportBufferTable);
            spin->setObjectName(QStringLiteral(
                "pref_new_project_buffer_%1_%2")
                    .arg(projectObjectName(kind),
                        QString::fromLatin1(
                            exportBufferFields[row].objectSuffix)));
            spin->setRange(0, std::numeric_limits<int>::max());
            spin->setValue(static_cast<int>(std::min<std::uint32_t>(
                configured.*(exportBufferFields[row].member),
                static_cast<std::uint32_t>(std::numeric_limits<int>::max()))));
            const bool isIjimMaterialsDefault =
                kind == ProjectType::IJIM
                && exportBufferFields[row].member
                    == &LevelExportBufferExtras::materials;
            spin->setToolTip(isIjimMaterialsDefault
                ? "For new IJIM levels, Materials Extra must be 0 or 64 for NDY "
                  "compatibility. 64 is the default and adds 64 slots beyond "
                  "the required dynamic MAT rows; 0 adds no spare MAT slots. "
                  "Other values block export."
                : "Additional allocation slots written beyond resources required "
                  "by the level. Zero adds no hidden reserve.");
            exportBufferTable->setCellWidget(
                static_cast<int>(row), static_cast<int>(project) + 1, spin);
            exportBufferSpins[project][row] = spin;
        }
    }
    exportBufferTable->resizeRowsToContents();
    const int completeExportBufferTableHeight =
        exportBufferTable->horizontalHeader()->height()
        + exportBufferTable->verticalHeader()->length()
        + exportBufferTable->frameWidth() * 2 + 2;
    exportBufferTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    exportBufferTable->setFixedHeight(completeExportBufferTableHeight);
    exportBufferSection->contentLayout()->addWidget(exportBufferTable);

    auto* restoreExportBufferDefaults = new QPushButton(
        "Restore original defaults", exportBufferSection);
    restoreExportBufferDefaults->setObjectName(
        "pref_restore_new_project_export_buffers");
    restoreExportBufferDefaults->setToolTip(
        "Restore the original SED new-project headroom for all three project types.");
    QObject::connect(restoreExportBufferDefaults, &QPushButton::clicked,
        &dialog, [exportBufferProjects, exportBufferFields, exportBufferSpins]()
        {
            for (std::size_t project = 0;
                 project < exportBufferProjects.size(); ++project)
            {
                const LevelExportBufferExtras defaults =
                    ui::project::defaultNewProjectExportBufferExtras(
                        exportBufferProjects[project]);
                for (std::size_t row = 0;
                     row < exportBufferFields.size(); ++row)
                {
                    exportBufferSpins[project][row]->setValue(
                        static_cast<int>(defaults.*(
                            exportBufferFields[row].member)));
                }
            }
        });
    exportBufferSection->contentLayout()->addWidget(
        restoreExportBufferDefaults, 0, Qt::AlignRight);
    resourcesLayout->addWidget(exportBufferSection);

    resourcesLayout->addStretch(1);

    auto* resourcesScroll = new QScrollArea(&dialog);
    resourcesScroll->setWidgetResizable(true);
    resourcesScroll->setFrameShape(QFrame::NoFrame);
    resourcesScroll->setWidget(resourcesPage);

    categories->addItem("Resources");
    stack->addWidget(resourcesScroll);

    // ---- Viewport preferences page ----
    auto* viewportPage = new QWidget(&dialog);
    viewportPage->setProperty(
        "sed.preferencesDisclosurePath", QStringLiteral("viewport"));
    auto* viewportLayout = new QVBoxLayout(viewportPage);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    viewportLayout->setSpacing(12);
    const ViewportBannerSettings originalViewportBannerSettings = glView_
        ? glView_->viewportBannerSettings()
        : loadViewportBannerSettings(settingsStore());
    QCheckBox* previewCubesCheck = nullptr;
    QCheckBox* editorGizmosCheck = nullptr;
    QCheckBox* previewGizmosCheck = nullptr;
    QCheckBox* editorArchGizmosCheck = nullptr;
    QCheckBox* previewArchGizmosCheck = nullptr;
    QColor iconCubeTint{ Qt::white };
    std::unordered_map<std::string, QColor> typeColors{};
    QColor viewportBgClear{ glView_ ? glView_->backgroundClearColor() : QColor::fromRgbF(0.08, 0.09, 0.12, 1.0) };
    QColor viewportWireColor{ glView_ ? glView_->wireframeColor() : QColor::fromRgbF(0.86, 0.87, 0.92, 1.0) };
    QColor viewportGridColor{ glView_ ? glView_->gridColor() : QColor::fromRgb(112, 128, 156) };
    QColor viewportGridMajorColor{ glView_ ? glView_->gridMajorColor() : QColor::fromRgb(170, 184, 208) };
    QColor viewportGridAxisXColor{ glView_ ? glView_->gridAxisXColor() : QColor(210, 75, 70) };
    QColor viewportGridAxisYColor{ glView_ ? glView_->gridAxisYColor() : QColor(95, 190, 95) };
    QColor viewportGridAxisZColor{ glView_ ? glView_->gridAxisZColor() : QColor(90, 140, 215) };
    QColor uvCanvasBackgroundColor{ QColor(15, 20, 29) };
    QColor uvGridMainColor{ QColor::fromRgb(66, 78, 96) };
    QColor uvGridSubColor{ QColor::fromRgb(48, 58, 74) };
    QColor uvGridAxisUColor{ QColor(160, 78, 78) };
    QColor uvGridAxisVColor{ QColor(78, 160, 112) };
    QColor viewportSelPrimary{ glView_ ? glView_->selectionPrimaryColor() : QColor::fromRgbF(0.98, 0.80, 0.25, 1.0) };
    QColor viewportSelMulti{ glView_ ? glView_->selectionMultiColor() : QColor::fromRgbF(0.20, 0.72, 0.95, 1.0) };
    QColor viewportSelEdgeMulti{ glView_ ? glView_->selectionEdgeMultiColor() : QColor::fromRgb(255, 152, 56) };
    QColor viewportSelActive{ glView_ ? glView_->selectionActiveColor() : QColor::fromRgbF(0.95, 0.20, 0.20, 1.0) };
    QColor viewportSelVertex{ glView_ ? glView_->selectionVertexColor() : QColor::fromRgbF(0.98, 0.80, 0.25, 1.0) };
    QColor viewportSelVertexMulti{ glView_ ? glView_->selectionVertexMultiColor() : QColor::fromRgbF(0.20, 0.72, 0.95, 1.0) };
    QColor viewportCogPickPrimary{ glView_ ? glView_->cogEntityPickPrimaryColor() : QColor::fromRgbF(0.20, 0.86, 0.96, 1.0) };
    QColor viewportCogPickActive{ glView_ ? glView_->cogEntityPickActiveColor() : QColor::fromRgbF(0.98, 0.66, 0.20, 1.0) };
    QString viewportColorTheme = QLatin1String(render::kViewportColorThemeNightBlue);
    std::vector<render::ViewportColorTheme> customViewportColorThemes{};
    std::vector<render::ViewportColorTheme> originalViewportThemeFiles{};
    std::vector<std::pair<QToolButton*, QColor*>> viewportThemeSwatches{};
    bool viewportThemeFilesDirty = false;
    bool cogThemeFilesDirty = false;
    bool viewportThemeDraftPending = false;
    bool originalViewportThemeKeyPresent = false;
    QString originalViewportThemeId{};
    std::function<bool(QString*)> persistViewportThemeFiles =
        [](QString*) { return true; };
    std::function<bool(QString*)> persistCogThemeFiles =
        [](QString*) { return true; };
    std::function<bool(QSettings&, QString*)> preflightCogThemeFiles =
        [](QSettings&, QString*) { return true; };
    const auto captureViewportPalette = [&]()
    {
        return render::ViewportColorPalette{
            viewportBgClear,
            viewportWireColor,
            viewportGridColor,
            viewportGridMajorColor,
            viewportGridAxisXColor,
            viewportGridAxisYColor,
            viewportGridAxisZColor,
            viewportSelPrimary,
            viewportSelMulti,
            viewportSelEdgeMulti,
            viewportSelActive,
            viewportSelVertex,
            viewportSelVertexMulti,
            viewportCogPickPrimary,
            viewportCogPickActive,
            uvCanvasBackgroundColor,
            uvGridMainColor,
            uvGridSubColor,
            uvGridAxisUColor,
            uvGridAxisVColor,
        };
    };
    const auto applyViewportPalette = [&](const render::ViewportColorPalette& palette)
    {
        viewportBgClear = palette.backgroundClear;
        viewportWireColor = palette.wireframe;
        viewportGridColor = palette.grid;
        viewportGridMajorColor = palette.gridMajor;
        viewportGridAxisXColor = palette.gridAxisX;
        viewportGridAxisYColor = palette.gridAxisY;
        viewportGridAxisZColor = palette.gridAxisZ;
        viewportSelPrimary = palette.selectionPrimary;
        viewportSelMulti = palette.selectionMulti;
        viewportSelEdgeMulti = palette.selectionEdgeMulti;
        viewportSelActive = palette.selectionActive;
        viewportSelVertex = palette.selectionVertex;
        viewportSelVertexMulti = palette.selectionVertexMulti;
        viewportCogPickPrimary = palette.cogPickPrimary;
        viewportCogPickActive = palette.cogPickActive;
        uvCanvasBackgroundColor = palette.uvCanvasBackground;
        uvGridMainColor = palette.uvGridMain;
        uvGridSubColor = palette.uvGridSub;
        uvGridAxisUColor = palette.uvGridAxisU;
        uvGridAxisVColor = palette.uvGridAxisV;
    };
    bool viewportOutlinerLayerColors = outlinerColorByLayer_;
    bool viewportWireLayerColors = wireframeColorByLayer_;
    double viewportWireLineWidthPx = glView_ ? static_cast<double>(glView_->wireframeLineWidthPx()) : 1.35;
    double viewportGridLineWidthPx = glView_ ? static_cast<double>(glView_->gridLineWidthPx()) : 1.0;
    bool viewportWireAntialiasing = glView_ ? glView_->wireframeAntialiasingEnabled() : true;
    int viewportMsaaSamples = 4;
    double viewportFramePathLineWorldSize = glView_ ? static_cast<double>(glView_->framePathLineWorldSize()) : 0.0025;
    double viewportFramePathPointWorldSize = glView_ ? static_cast<double>(glView_->framePathPointWorldSize()) : 0.0050;
    double gizmoStackSpacing = glView_
        ? static_cast<double>(glView_->thingGizmoStackSpacing())
        : static_cast<double>(render::kThingGizmoStackSpacingDefault);
    double thingLightGizmoOffset = glView_
        ? static_cast<double>(glView_->thingLightGizmoOffsetAboveModel()) : 0.0;
    double archThingGizmoOffset = glView_
        ? static_cast<double>(glView_->archThingGizmoOffsetAboveModel()) : 0.0;
    bool editorArchThingGizmosEnabled = glView_
        ? glView_->editorDefaultShowArchThingGizmos() : true;
    bool previewArchThingGizmosEnabled = glView_
        ? glView_->previewDefaultShowArchThingGizmos() : false;
    double levelGeometryVertexMarkerSizePx = glView_
        ? static_cast<double>(glView_->levelGeometryVertexMarkerSizePx())
        : static_cast<double>(GlView::kLevelGeometryVertexMarkerDefaultSizePx);
    double architecturalLightingMarkerSizePx = glView_
        ? static_cast<double>(glView_->architecturalLightingMarkerSizePx())
        : static_cast<double>(GlView::kArchitecturalLightingMarkerDefaultSizePx);
    bool architecturalLightingMarkerUseAuthoredColor = glView_
        ? glView_->architecturalLightingMarkerUseAuthoredColor()
        : true;
    QColor architecturalLightingMarkerColor = glView_
        ? glView_->architecturalLightingMarkerColor()
        : QColor::fromRgb(255, 204, 51);
    {
        const QSettings markerSettings = settingsStore();
        gizmoStackSpacing = markerSettings.value(
            "viewport/thing_gizmo_stack_spacing",
            gizmoStackSpacing).toDouble();
        thingLightGizmoOffset = markerSettings.value(
            "viewport/thing_light_gizmo_offset_above_model",
            thingLightGizmoOffset).toDouble();
        archThingGizmoOffset = markerSettings.value(
            "viewport/arch_thing_gizmo_offset_above_model",
            archThingGizmoOffset).toDouble();
        editorArchThingGizmosEnabled = markerSettings.value(
            "viewport/arch_thing_gizmos",
            editorArchThingGizmosEnabled).toBool();
        previewArchThingGizmosEnabled = markerSettings.value(
            "viewport/preview_arch_thing_gizmos",
            previewArchThingGizmosEnabled).toBool();
        levelGeometryVertexMarkerSizePx = markerSettings.value(
            "viewport/level_geometry_vertex_marker_size_px",
            levelGeometryVertexMarkerSizePx).toDouble();
        architecturalLightingMarkerSizePx = markerSettings.value(
            "viewport/architectural_lighting_marker_size_px",
            architecturalLightingMarkerSizePx).toDouble();
        architecturalLightingMarkerUseAuthoredColor = markerSettings.value(
            "viewport/architectural_lighting_marker_use_authored_color",
            architecturalLightingMarkerUseAuthoredColor).toBool();
        const QColor parsed(markerSettings.value(
            "viewport/architectural_lighting_marker_color",
            architecturalLightingMarkerColor.name(QColor::HexRgb)).toString().trimmed());
        if (parsed.isValid())
        {
            architecturalLightingMarkerColor = parsed;
        }
    }
    gizmoStackSpacing = std::isfinite(gizmoStackSpacing)
        ? std::clamp(gizmoStackSpacing, 0.0, 1.0)
        : static_cast<double>(render::kThingGizmoStackSpacingDefault);
    thingLightGizmoOffset = std::isfinite(thingLightGizmoOffset)
        ? std::clamp(thingLightGizmoOffset, 0.0, 100.0)
        : 0.0;
    archThingGizmoOffset = std::isfinite(archThingGizmoOffset)
        ? std::clamp(archThingGizmoOffset, 0.0, 100.0)
        : 0.0;
    levelGeometryVertexMarkerSizePx = std::isfinite(levelGeometryVertexMarkerSizePx)
        ? std::clamp(
            levelGeometryVertexMarkerSizePx,
            static_cast<double>(GlView::kLevelGeometryVertexMarkerMinSizePx),
            static_cast<double>(GlView::kLevelGeometryVertexMarkerMaxSizePx))
        : static_cast<double>(GlView::kLevelGeometryVertexMarkerDefaultSizePx);
    architecturalLightingMarkerSizePx = std::isfinite(
        architecturalLightingMarkerSizePx)
        ? std::clamp(
            architecturalLightingMarkerSizePx,
            static_cast<double>(GlView::kArchitecturalLightingMarkerMinSizePx),
            static_cast<double>(GlView::kArchitecturalLightingMarkerMaxSizePx))
        : static_cast<double>(GlView::kArchitecturalLightingMarkerDefaultSizePx);

    // Rendering section.
    auto* renderingSection = createCollapsedSection(
        QStringLiteral("rendering"), "Rendering", viewportPage);
    {
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        auto* dynamicLightShadingCombo = new QComboBox(renderingSection);
        dynamicLightShadingCombo->setObjectName("pref_dynamic_light_shading");
        dynamicLightShadingCombo->addItem(
            "Gouraud (per-vertex)",
            static_cast<int>(GlView::DynamicLightShading::Gouraud));
        dynamicLightShadingCombo->addItem(
            "Phong (per-pixel diffuse)",
            static_cast<int>(GlView::DynamicLightShading::Phong));
        dynamicLightShadingCombo->setToolTip(
            "Choose how dynamic lights shade Gouraud geometry. Gouraud samples at vertices; "
            "Phong shades each pixel. Baked lighting is unchanged.");
        const int dynamicLightShading = static_cast<int>(
            glView_ ? glView_->dynamicLightShading() : GlView::DynamicLightShading::Gouraud);
        dynamicLightShadingCombo->setCurrentIndex(std::max(
            0,
            dynamicLightShadingCombo->findData(dynamicLightShading)));
        form->addRow("Dynamic lights", dynamicLightShadingCombo);

        renderingSection->contentLayout()->addLayout(form);
    }
    viewportLayout->addWidget(renderingSection);

    // Things section (cubes, colors, and collision-preview options).
    auto* thingSection = createCollapsedSection(
        QStringLiteral("things"), "Things", viewportPage);
    {
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        auto* cubeSizeSpin = new QDoubleSpinBox(thingSection);
        cubeSizeSpin->setObjectName("pref_cube_size");
        cubeSizeSpin->setRange(0.001, 0.05);
        cubeSizeSpin->setDecimals(4);
        cubeSizeSpin->setSingleStep(0.005);
        cubeSizeSpin->setSuffix(" units");
        cubeSizeSpin->setValue(static_cast<double>(glView_ ? glView_->thingCubeHalfSize() : 0.0125f));
        const auto refreshCubeSizeToolTip = [cubeSizeSpin]
        {
            cubeSizeSpin->setToolTip(QStringLiteral(
                "Half-size of the cube drawn for non-visual things (max %1)")
                .arg(ui::formatUiNumber(0.05, 'f', 2)));
        };
        refreshCubeSizeToolTip();
        ui::onUiNumericFormatChanged(cubeSizeSpin, refreshCubeSizeToolTip);
        form->addRow("Cube half-size", cubeSizeSpin);

        // Cube base color.
        {
            QSettings settings = settingsStore();
            if (settings.contains("viewport/thing_icon_cube_tint"))
            {
                const QColor parsed(settings.value("viewport/thing_icon_cube_tint").toString().trimmed());
                if (parsed.isValid())
                {
                    iconCubeTint = parsed;
                }
            }
            else if (glView_)
            {
                iconCubeTint = glView_->thingIconCubeTintColor();
            }

            auto* swatch = new QToolButton(thingSection);
            swatch->setObjectName("pref_icon_cube_tint");
            swatch->setAutoRaise(true);
            swatch->setCursor(Qt::PointingHandCursor);
            swatch->setFixedSize(46, 18);
            swatch->setToolTip(iconCubeTint.name(QColor::HexRgb));

            auto updateSwatch = [](QToolButton* b, const QColor& c)
            {
                if (!b)
                {
                    return;
                }
                b->setToolTip(c.name(QColor::HexRgb));
                b->setStyleSheet(QString(
                    "QToolButton {"
                    "  background: %1;"
                    "  border: 1px solid rgba(0, 0, 0, 160);"
                    "  border-radius: 4px;"
                    "}"
                    "QToolButton:hover {"
                    "  border-color: rgba(255, 255, 255, 140);"
                    "}").arg(c.name(QColor::HexRgb)));
            };

            updateSwatch(swatch, iconCubeTint);

            QObject::connect(swatch, &QToolButton::clicked, &dialog, [&]()
                {
                    const QColor chosen = QColorDialog::getColor(iconCubeTint, &dialog, "Select cube base color");
                    if (!chosen.isValid())
                    {
                        return;
                    }
                    iconCubeTint = chosen;
                    updateSwatch(swatch, chosen);
                });

            form->addRow("Cube base color", swatch);
        }

        previewCubesCheck = new QCheckBox("Show cubes for non-visual things in preview mode", thingSection);
        previewCubesCheck->setChecked(glView_ ? glView_->showPreviewCubes() : false);
        form->addRow("Preview cubes", previewCubesCheck);

        thingSection->contentLayout()->addLayout(form);

        auto* motsTreeCollisionSection = createCollapsedSection(
            QStringLiteral("mots-tree-collision"),
            "Collision preview",
            thingSection);
        motsTreeCollisionSection->setObjectName(
            "pref_section_mots_tree_collision_preview");
        motsTreeCollisionSection->contentLayout()->addWidget(
            addProjectBadge(
                motsTreeCollisionSection,
                QStringLiteral("pref_mots_tree_collision_project_badge"),
                QStringLiteral("MotS only"),
                QStringLiteral("Applies to Mysteries of the Sith projects")),
            0,
            Qt::AlignLeft);
        auto* motsTreeCollisionForm = new QFormLayout();
        motsTreeCollisionForm->setContentsMargins(6, 4, 6, 4);
        motsTreeCollisionForm->setSpacing(6);
        auto* motsTreeCollisionRadiusFactor =
            new QDoubleSpinBox(motsTreeCollisionSection);
        motsTreeCollisionRadiusFactor->setObjectName(
            "pref_mots_tree_collision_preview_mesh_radius_factor");
        motsTreeCollisionRadiusFactor->setRange(0.01, 10.0);
        motsTreeCollisionRadiusFactor->setDecimals(2);
        motsTreeCollisionRadiusFactor->setSingleStep(0.05);
        motsTreeCollisionRadiusFactor->setSuffix(" x");
        double motsTreeCollisionRadiusFactorValue = 0.75;
        {
            const QSettings settings = settingsStore();
            bool storedFactorParsed = false;
            const double storedFactor = settings.value(
                "viewport/mots_tree_collision_preview_mesh_radius_factor",
                0.75).toDouble(&storedFactorParsed);
            if (storedFactorParsed && std::isfinite(storedFactor))
            {
                motsTreeCollisionRadiusFactorValue = std::clamp(
                    storedFactor, 0.01, 10.0);
            }
        }
        motsTreeCollisionRadiusFactor->setValue(
            motsTreeCollisionRadiusFactorValue);
        const auto refreshMotsTreeCollisionToolTip = [motsTreeCollisionRadiusFactor]
        {
            motsTreeCollisionRadiusFactor->setToolTip(QStringLiteral(
                "MotS collide=2 tree preview only. treesize sets the broad-phase radius; each hierarchy node normally uses "
                "%1 x its authored MESH RADIUS. Flagged 0x2 hierarchy nodes inherit an ancestor radius after rendering. "
                "This setting changes only the displayed node spheres, never level data or exports.")
                .arg(ui::formatUiNumber(0.75, 'f', 2)));
        };
        refreshMotsTreeCollisionToolTip();
        ui::onUiNumericFormatChanged(motsTreeCollisionRadiusFactor,
            refreshMotsTreeCollisionToolTip);
        motsTreeCollisionForm->addRow(
            "Node mesh radius scale", motsTreeCollisionRadiusFactor);
        motsTreeCollisionSection->contentLayout()->addLayout(
            motsTreeCollisionForm);
        thingSection->contentLayout()->addWidget(motsTreeCollisionSection);

        // Nested color settings.
        auto* colorsSection = createCollapsedSection(
            QStringLiteral("colors"), "Colors", thingSection);
        {
            auto* colorsForm = new QFormLayout();
            colorsForm->setContentsMargins(6, 4, 6, 4);
            colorsForm->setSpacing(6);

            struct ColorDef
            {
                const char* key;
                const char* label;
                QColor def;
            };

            const std::vector<ColorDef> defs = {
                {"camera", "Camera", QColor(77, 153, 255)},
                {"actor", "Actor", QColor(230, 89, 51)},
                {"weapon", "Weapon", QColor(242, 153, 38)},
                {"item", "Item", QColor(51, 217, 77)},
                {"player", "Player", QColor(26, 230, 230)},
                {"cog", "COG", QColor(204, 128, 230)},
                {"ghost", "Ghost", QColor(153, 153, 166)},
                {"corpse", "Corpse", QColor(140, 89, 51)},
                {"explosion", "Explosion", QColor(255, 102, 26)},
                {"particle", "Particle", QColor(242, 242, 77)},
                {"debris", "Debris", QColor(166, 128, 89)},
                {"hint", "Hint", QColor(102, 230, 204)},
                {"sprite", "Sprite", QColor(217, 179, 217)},
                {"polyline", "Polyline", QColor(191, 191, 102)},
                {"free", "Free", QColor(128, 128, 128)},
                {"default", "Default", QColor(179, 179, 179)},
            };

            QSettings settings = settingsStore();

            auto loadColor = [&](const char* key, const QColor& fallback) -> QColor
            {
                const QString k = QString("viewport/thing_type_color/%1").arg(QString::fromUtf8(key));
                const QString v = settings.value(k).toString().trimmed();
                if (!v.isEmpty())
                {
                    const QColor parsed(v);
                    if (parsed.isValid())
                    {
                        return parsed;
                    }
                }
                return fallback;
            };

            auto* rowsWidget = new QWidget(colorsSection);
            auto* rowsLayout = new QGridLayout(rowsWidget);
            rowsLayout->setContentsMargins(0, 0, 0, 0);
            rowsLayout->setHorizontalSpacing(10);
            rowsLayout->setVerticalSpacing(6);

            int row = 0;
            for (const ColorDef& def : defs)
            {
                const QColor initial = loadColor(def.key, def.def);
                typeColors[def.key] = initial;

                auto* label = new QLabel(def.label, rowsWidget);
                rowsLayout->addWidget(label, row, 0);

                auto* swatch = new QToolButton(rowsWidget);
                swatch->setAutoRaise(true);
                swatch->setCursor(Qt::PointingHandCursor);
                swatch->setFixedSize(46, 18);
                swatch->setToolTip(initial.name(QColor::HexRgb));

                auto updateSwatch = [](QToolButton* b, const QColor& c)
                {
                    if (!b)
                    {
                        return;
                    }
                    b->setToolTip(c.name(QColor::HexRgb));
                    b->setStyleSheet(QString(
                        "QToolButton {"
                        "  background: %1;"
                        "  border: 1px solid rgba(0, 0, 0, 160);"
                        "  border-radius: 4px;"
                        "}"
                        "QToolButton:hover {"
                        "  border-color: rgba(255, 255, 255, 140);"
                        "}").arg(c.name(QColor::HexRgb)));
                };

                updateSwatch(swatch, initial);

                QObject::connect(swatch, &QToolButton::clicked, &dialog, [&, swatch, key = std::string(def.key)]()
                    {
                        const QColor current = typeColors.contains(key) ? typeColors.at(key) : QColor(179, 179, 179);
                        const QColor chosen = QColorDialog::getColor(current, &dialog, "Select color");
                        if (!chosen.isValid())
                        {
                            return;
                        }

                        typeColors[key] = chosen;
                        updateSwatch(swatch, chosen);
                    });

                rowsLayout->addWidget(swatch, row, 1);
                ++row;
            }

            colorsForm->addRow(rowsWidget);
            colorsSection->contentLayout()->addLayout(colorsForm);
        }

        thingSection->contentLayout()->addWidget(colorsSection);
    }
    viewportLayout->addWidget(thingSection);

    // Gizmos section (type hints + lights).
    auto* gizmosSection = createCollapsedSection(
        QStringLiteral("gizmos"), "Gizmos", viewportPage);
    {
        auto* spacingForm = new QFormLayout();
        spacingForm->setContentsMargins(6, 4, 6, 4);
        spacingForm->setSpacing(6);
        auto* gizmoStackSpacingSpin = new QDoubleSpinBox(gizmosSection);
        gizmoStackSpacingSpin->setObjectName("pref_gizmo_stack_spacing");
        gizmoStackSpacingSpin->setRange(0.0, 1.0);
        gizmoStackSpacingSpin->setDecimals(4);
        gizmoStackSpacingSpin->setSingleStep(0.001);
        gizmoStackSpacingSpin->setSuffix(" units");
        gizmoStackSpacingSpin->setValue(gizmoStackSpacing);
        gizmoStackSpacingSpin->setToolTip(
            "World-space gap between stacked Thing Type, Light, and Arch Thing markers in every project.");
        spacingForm->addRow("Stack spacing", gizmoStackSpacingSpin);
        gizmosSection->contentLayout()->addLayout(spacingForm);

        auto* typeHintSection = createCollapsedSection(
            QStringLiteral("thing-type"), "Thing Type", gizmosSection);
        typeHintSection->setObjectName("pref_section_thing_type_gizmos");
        {
            auto* form = new QFormLayout();
            form->setContentsMargins(6, 4, 6, 4);
            form->setSpacing(6);

            auto* gizmoSizeSpin = new QDoubleSpinBox(typeHintSection);
            gizmoSizeSpin->setObjectName("pref_gizmo_world_size");
            gizmoSizeSpin->setRange(0.001, 0.2);
            gizmoSizeSpin->setDecimals(4);
            gizmoSizeSpin->setSingleStep(0.001);
            gizmoSizeSpin->setSuffix(" units");
            gizmoSizeSpin->setValue(static_cast<double>(glView_ ? glView_->thingGizmoWorldSize() : 0.03f));
            gizmoSizeSpin->setToolTip("Fixed world-space size of Thing type gizmo icons.");
            form->addRow("Size", gizmoSizeSpin);

            auto* gizmoHeightSpin = new QDoubleSpinBox(typeHintSection);
            gizmoHeightSpin->setObjectName("pref_gizmo_model_top_height");
            gizmoHeightSpin->setRange(0.0, 100.0);
            gizmoHeightSpin->setDecimals(4);
            gizmoHeightSpin->setSingleStep(0.01);
            gizmoHeightSpin->setSuffix(" units");
            gizmoHeightSpin->setValue(static_cast<double>(
                glView_ ? glView_->thingGizmoHeightAboveModel() : 0.0f));
            gizmoHeightSpin->setToolTip(
                "Additional world-space offset for the complete Thing-attached marker stack. Marker icons are raised above the model to clear their height and shared stack spacing, even at zero offset.");
            form->addRow("Offset", gizmoHeightSpin);

            editorGizmosCheck = new QCheckBox("Enable in editor mode", typeHintSection);
            editorGizmosCheck->setChecked(glView_ ? glView_->editorDefaultShowThingGizmos() : false);
            form->addRow("Editor", editorGizmosCheck);

            previewGizmosCheck = new QCheckBox("Enable in game preview mode", typeHintSection);
            previewGizmosCheck->setChecked(glView_ ? glView_->previewDefaultShowThingGizmos() : false);
            form->addRow("Preview", previewGizmosCheck);

            typeHintSection->contentLayout()->addLayout(form);
        }

        gizmosSection->contentLayout()->addWidget(typeHintSection);

        // Light gizmos.
        auto* lightSection = createCollapsedSection(
            QStringLiteral("light"), "Light", gizmosSection);
        lightSection->setObjectName("pref_section_light_gizmos");
        {
            auto* form = new QFormLayout();
            form->setContentsMargins(6, 4, 6, 4);
            form->setSpacing(6);

            auto* lightSizeSpin = new QDoubleSpinBox(lightSection);
            lightSizeSpin->setObjectName("pref_light_gizmo_world_size");
            lightSizeSpin->setRange(0.005, 0.25);
            lightSizeSpin->setDecimals(4);
            lightSizeSpin->setSingleStep(0.005);
            lightSizeSpin->setSuffix(" units");
            lightSizeSpin->setValue(static_cast<double>(glView_ ? glView_->lightGizmoWorldSize() : 0.04f));
            lightSizeSpin->setToolTip("Fixed world-space size of light gizmo sprites.");
            form->addRow("Sprite size", lightSizeSpin);

            auto* lightUseColorCheck = new QCheckBox("Tint with light color", lightSection);
            lightUseColorCheck->setObjectName("pref_light_gizmo_use_light_color");
            lightUseColorCheck->setToolTip("When enabled, light gizmo sprites inherit the light's color.");
            {
                QSettings settings = settingsStore();
                lightUseColorCheck->setChecked(settings.value("viewport/light_gizmo_use_light_color", true).toBool());
            }
            form->addRow("Color", lightUseColorCheck);

            auto* thingLightOffsetSpin = new QDoubleSpinBox(lightSection);
            thingLightOffsetSpin->setObjectName("pref_thing_light_gizmo_offset");
            thingLightOffsetSpin->setRange(0.0, 100.0);
            thingLightOffsetSpin->setDecimals(4);
            thingLightOffsetSpin->setSingleStep(0.01);
            thingLightOffsetSpin->setSuffix(" units");
            thingLightOffsetSpin->setValue(thingLightGizmoOffset);
            thingLightOffsetSpin->setToolTip(
                "Extra world-space gap above the Thing model for regular Thing-light icons. "
                "Upper ARCH markers move with the stack; actor Headlights stay at their emitter.");
            form->addRow("Thing Light offset", thingLightOffsetSpin);

            lightSection->contentLayout()->addLayout(form);
        }

        gizmosSection->contentLayout()->addWidget(lightSection);

        {
            auto* archThingSection = createCollapsedSection(
                QStringLiteral("arch-thing"), "Arch Thing", gizmosSection);
            archThingSection->setObjectName("pref_section_arch_thing_gizmos");
            auto* applicabilityBadge = addProjectBadge(
                archThingSection,
                QStringLiteral("pref_arch_thing_project_badge"),
                QStringLiteral("JKDF2 / MotS only"),
                QStringLiteral("Applies to JKDF2 and MotS projects"));
            archThingSection->contentLayout()->addWidget(
                applicabilityBadge, 0, Qt::AlignLeft);
            auto* form = new QFormLayout();
            form->setContentsMargins(6, 4, 6, 4);
            form->setSpacing(6);

            editorArchGizmosCheck = new QCheckBox(
                "Enable in editor mode", archThingSection);
            editorArchGizmosCheck->setToolTip(
                "Controls Arch Thing markers for flag 0x8 in JKDF2/MotS. This general preference has no effect in IJIM.");
            editorArchGizmosCheck->setObjectName(
                "pref_arch_thing_gizmo_editor_enabled");
            editorArchGizmosCheck->setChecked(editorArchThingGizmosEnabled);
            form->addRow("Editor", editorArchGizmosCheck);

            previewArchGizmosCheck = new QCheckBox(
                "Enable in game preview mode", archThingSection);
            previewArchGizmosCheck->setToolTip(
                "Controls Arch Thing markers for flag 0x8 in JKDF2/MotS. This general preference has no effect in IJIM.");
            previewArchGizmosCheck->setObjectName(
                "pref_arch_thing_gizmo_preview_enabled");
            previewArchGizmosCheck->setChecked(previewArchThingGizmosEnabled);
            form->addRow("Game Preview", previewArchGizmosCheck);

            auto* archOffsetSpin = new QDoubleSpinBox(archThingSection);
            archOffsetSpin->setObjectName("pref_arch_thing_gizmo_offset");
            archOffsetSpin->setRange(0.0, 100.0);
            archOffsetSpin->setDecimals(4);
            archOffsetSpin->setSingleStep(0.01);
            archOffsetSpin->setSuffix(" units");
            archOffsetSpin->setValue(archThingGizmoOffset);
            archOffsetSpin->setToolTip(
                "Additional world-space clearance above Arch Thing markers in JKDF2/MotS. The marker is automatically raised by its icon height and the shared stack spacing, even at zero offset. This general preference has no effect in IJIM.");
            form->addRow("Offset", archOffsetSpin);

            archThingSection->contentLayout()->addLayout(form);
            gizmosSection->contentLayout()->addWidget(archThingSection);
        }


    }

    viewportLayout->addWidget(gizmosSection);

    auto* selectionMarkersSection = createCollapsedSection(
        QStringLiteral("selection-markers"), "Selection markers", viewportPage);
    selectionMarkersSection->setObjectName("pref_section_selection_markers");
    {
        auto* geometryVertexSection = createCollapsedSection(
            QStringLiteral("level-geometry-vertices"),
            "Level Geometry Vertices",
            selectionMarkersSection);
        geometryVertexSection->setObjectName("pref_section_level_geometry_vertices");
        auto* geometryVertexForm = new QFormLayout();
        geometryVertexForm->setContentsMargins(6, 4, 6, 4);
        geometryVertexForm->setSpacing(6);
        auto* geometryVertexSize = new QDoubleSpinBox(geometryVertexSection);
        geometryVertexSize->setObjectName(
            "pref_level_geometry_vertex_marker_size_px");
        geometryVertexSize->setRange(
            GlView::kLevelGeometryVertexMarkerMinSizePx,
            GlView::kLevelGeometryVertexMarkerMaxSizePx);
        geometryVertexSize->setDecimals(0);
        geometryVertexSize->setSingleStep(1.0);
        geometryVertexSize->setSuffix(" px");
        geometryVertexSize->setValue(levelGeometryVertexMarkerSizePx);
        geometryVertexSize->setToolTip(
            "Base screen-space size of level-geometry vertex points. "
            "Selected and active vertices scale with it and remain larger.");
        geometryVertexForm->addRow("Marker size", geometryVertexSize);
        geometryVertexSection->contentLayout()->addLayout(geometryVertexForm);
        selectionMarkersSection->contentLayout()->addWidget(geometryVertexSection);

        {
            auto* architecturalVertexSection = createCollapsedSection(
                QStringLiteral("arch-vertices"),
                "ARCH vertices",
                selectionMarkersSection);
            {
                architecturalVertexSection->setObjectName("pref_section_arch_vertices");
                auto* applicabilityBadge = addProjectBadge(
                    architecturalVertexSection,
                    QStringLiteral("pref_arch_vertices_project_badge"),
                    QStringLiteral("MotS only"),
                    QStringLiteral("Applies to Mysteries of the Sith projects"));
                architecturalVertexSection->contentLayout()->addWidget(
                    applicabilityBadge, 0, Qt::AlignLeft);
                auto* form = new QFormLayout();
                form->setContentsMargins(6, 4, 6, 4);
                form->setSpacing(6);

                auto* markerSizeSpin = new QDoubleSpinBox(architecturalVertexSection);
                markerSizeSpin->setObjectName(
                    "pref_architectural_lighting_marker_size_px");
                markerSizeSpin->setRange(
                    GlView::kArchitecturalLightingMarkerMinSizePx,
                    GlView::kArchitecturalLightingMarkerMaxSizePx);
                markerSizeSpin->setDecimals(0);
                markerSizeSpin->setSingleStep(1.0);
                markerSizeSpin->setSuffix(" px");
                markerSizeSpin->setValue(architecturalLightingMarkerSizePx);
                markerSizeSpin->setToolTip(
                    "Screen-space size of MotS ARCHLIGHTING vertex markers. "
                    "The marker and its hit radius follow this value and scale with display DPI.");
                form->addRow("Marker size", markerSizeSpin);

                auto* markerUseAuthoredCheck = new QCheckBox(
                    "Use authored vertex color", architecturalVertexSection);
                markerUseAuthoredCheck->setObjectName(
                    "pref_architectural_lighting_marker_use_authored_color");
                markerUseAuthoredCheck->setChecked(
                    architecturalLightingMarkerUseAuthoredColor);
                markerUseAuthoredCheck->setToolTip(
                    "Color each marker from the selected model vertex's authored "
                    "ARCHLIGHTING RGB channels. Turn this off to use the fallback color below.");
                form->addRow("Color source", markerUseAuthoredCheck);

                auto* markerColorSwatch = new QToolButton(architecturalVertexSection);
                markerColorSwatch->setObjectName(
                    "pref_architectural_lighting_marker_color");
                markerColorSwatch->setAutoRaise(true);
                markerColorSwatch->setCursor(Qt::PointingHandCursor);
                markerColorSwatch->setFixedSize(46, 18);
                markerColorSwatch->setProperty(
                    "sed.colorValue", architecturalLightingMarkerColor);
                const auto updateMarkerColorSwatch = [](
                    QToolButton* button,
                    const QColor& color)
                {
                    if (!button)
                    {
                        return;
                    }
                    button->setProperty("sed.colorValue", color);
                    button->setToolTip(color.name(QColor::HexRgb));
                    button->setStyleSheet(QString(
                        "QToolButton {"
                        "  background: %1;"
                        "  border: 1px solid rgba(0, 0, 0, 160);"
                        "  border-radius: 4px;"
                        "}"
                        "QToolButton:hover {"
                        "  border-color: rgba(255, 255, 255, 140);"
                        "}").arg(color.name(QColor::HexRgb)));
                };
                updateMarkerColorSwatch(
                    markerColorSwatch, architecturalLightingMarkerColor);
                markerColorSwatch->setEnabled(!architecturalLightingMarkerUseAuthoredColor);
                QObject::connect(
                    markerUseAuthoredCheck,
                    &QCheckBox::toggled,
                    markerColorSwatch,
                    [markerColorSwatch](const bool checked)
                    {
                        markerColorSwatch->setEnabled(!checked);
                    });
                QObject::connect(
                    markerColorSwatch,
                    &QToolButton::clicked,
                    &dialog,
                    [&]()
                    {
                        const QColor chosen = QColorDialog::getColor(
                            architecturalLightingMarkerColor,
                            &dialog,
                            "Select ARCHLIGHTING fallback marker color");
                        if (!chosen.isValid())
                        {
                            return;
                        }
                        architecturalLightingMarkerColor = chosen;
                        updateMarkerColorSwatch(
                            markerColorSwatch, architecturalLightingMarkerColor);
                    });
                form->addRow("Fallback color", markerColorSwatch);

                architecturalVertexSection->contentLayout()->addLayout(form);
            }

            selectionMarkersSection->contentLayout()->addWidget(architecturalVertexSection);
        }
    }
    viewportLayout->addWidget(selectionMarkersSection);

    // Asset preview section.
    auto* previewSection = createCollapsedSection(
        QStringLiteral("asset-preview"), "Asset Preview", viewportPage);
    {
        QSettings settings = settingsStore();

        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        auto* autoRotateCheck = new QCheckBox("Rotate preview assets", previewSection);
        autoRotateCheck->setObjectName("pref_asset_preview_auto_rotate");
        autoRotateCheck->setChecked(settings.value("preview/asset_auto_rotate_enabled", true).toBool());
        form->addRow("Auto-rotate", autoRotateCheck);

        auto* rotateSpeedSpin = new QDoubleSpinBox(previewSection);
        rotateSpeedSpin->setObjectName("pref_asset_preview_auto_rotate_speed");
        rotateSpeedSpin->setDecimals(0);
        rotateSpeedSpin->setRange(0.0, 720.0);
        rotateSpeedSpin->setSingleStep(15.0);
        rotateSpeedSpin->setSuffix(" deg/s");
        rotateSpeedSpin->setValue(std::clamp(
            settings.value("preview/asset_auto_rotate_speed_deg_per_sec", 72.0).toDouble(),
            0.0,
            720.0));
        rotateSpeedSpin->setEnabled(autoRotateCheck->isChecked());
        form->addRow("Speed", rotateSpeedSpin);

        auto* templateThumbMode = new QComboBox(previewSection);
        templateThumbMode->setObjectName("pref_template_list_model_thumb_mode");
        templateThumbMode->addItem("Shaded", 0);
        templateThumbMode->addItem("Wireframe", 1);
        {
            const int storedMode =
                settings.value("preview/template_list_3do_thumb_mode", 0).toInt() == 1
                ? 1
                : 0;
            const int idx = std::max(0, templateThumbMode->findData(storedMode));
            templateThumbMode->setCurrentIndex(idx);
        }
        form->addRow("Template 3DO thumbs", templateThumbMode);

        auto* allowBaseTemplatesCheck = new QCheckBox("Allow templates starting with '_'", previewSection);
        allowBaseTemplatesCheck->setObjectName("pref_allow_base_templates");
        allowBaseTemplatesCheck->setChecked(settings.value("templates/allow_base_template_pick", false).toBool());
        allowBaseTemplatesCheck->setToolTip("When disabled, underscore-prefixed base templates are hidden from thing template picking.");
        form->addRow("Thing base templates", allowBaseTemplatesCheck);

        QObject::connect(autoRotateCheck, &QCheckBox::toggled, rotateSpeedSpin, [rotateSpeedSpin](const bool checked)
            {
                rotateSpeedSpin->setEnabled(checked);
            });

        previewSection->contentLayout()->addLayout(form);
    }

    viewportLayout->addWidget(previewSection);

    // Rendering colors section.
    auto* viewportColorsSection = createCollapsedSection(
        QStringLiteral("viewport-colors"), "Appearance", viewportPage);
    viewportColorsSection->setObjectName("pref_viewport_appearance_section");
    {
        QSettings settings = settingsStore();

        const render::ViewportColorThemeSelection selection =
            render::resolveViewportColorTheme(settings);
        viewportColorTheme = selection.id;
        originalViewportThemeKeyPresent = settings.contains(
            QString::fromLatin1(render::kViewportColorThemeSetting));
        originalViewportThemeId = settings.value(
            QString::fromLatin1(render::kViewportColorThemeSetting)).toString();
        applyViewportPalette(selection.palette);
        QString viewportThemeLoadError{};
        customViewportColorThemes =
            render::loadCustomViewportColorThemes(
                settings, &viewportThemeLoadError);
        originalViewportThemeFiles = customViewportColorThemes;
        if (!viewportThemeLoadError.isEmpty())
        {
            QMessageBox::warning(
                &dialog,
                QStringLiteral("Viewport Themes Could Not Be Fully Loaded"),
                viewportThemeLoadError);
        }
        viewportOutlinerLayerColors = settings.value("viewport/layers/color_outliner_entities", viewportOutlinerLayerColors).toBool();
        viewportWireLayerColors = settings.value("viewport/layers/color_wireframe", viewportWireLayerColors).toBool();
        viewportWireLineWidthPx = settings.value("viewport/wireframe_line_width_px", viewportWireLineWidthPx).toDouble();
        viewportGridLineWidthPx = settings.value("viewport/grid_line_width_px", viewportGridLineWidthPx).toDouble();
        viewportWireAntialiasing = settings.value("viewport/wireframe_antialiasing", viewportWireAntialiasing).toBool();
        bool msaaSamplesOk = false;
        const int storedMsaaSamples = settings.value("viewport/msaa_samples", 4).toInt(&msaaSamplesOk);
        if ( msaaSamplesOk
            && ( storedMsaaSamples == 0 || storedMsaaSamples == 2
                || storedMsaaSamples == 4 || storedMsaaSamples == 8
                || storedMsaaSamples == 16 ) )
        {
            viewportMsaaSamples = storedMsaaSamples;
        }
        viewportFramePathLineWorldSize = settings.value("viewport/frame_path_line_world_size", viewportFramePathLineWorldSize).toDouble();
        viewportFramePathPointWorldSize = settings.value("viewport/frame_path_point_world_size", viewportFramePathPointWorldSize).toDouble();

        auto* themeGroup = new QGroupBox("Theme", viewportColorsSection);
        auto* themeLayout = new QVBoxLayout(themeGroup);
        themeLayout->setContentsMargins(8, 8, 8, 8);
        auto* themeForm = new QFormLayout();
        themeForm->setContentsMargins(0, 0, 0, 0);
        themeForm->setSpacing(6);
        auto* themeCombo = new QComboBox(themeGroup);
        themeCombo->setObjectName("pref_viewport_color_theme");
        const auto serializeTheme = [](const render::ViewportColorTheme& theme)
        {
            QVariantMap value{};
            value.insert(QStringLiteral("id"), theme.id);
            value.insert(QStringLiteral("label"), theme.label);
            for ( const auto& [key, member] : render::kViewportColorPaletteFields )
            {
                value.insert(QLatin1String(key),
                    (theme.palette.*member).name(QColor::HexRgb));
            }
            return value;
        };
        const auto deserializeTheme = [](const QVariantMap& value)
        {
            render::ViewportColorTheme theme{};
            theme.id = value.value(QStringLiteral("id")).toString().trimmed();
            theme.label = value.value(QStringLiteral("label")).toString().trimmed();
            theme.palette = render::nightBlueViewportColorPalette();
            for ( const auto& [key, member] : render::kViewportColorPaletteFields )
            {
                const QColor color(value.value(QLatin1String(key)).toString());
                if ( color.isValid() )
                {
                    theme.palette.*member = color;
                }
            }
            return theme;
        };
        QVariantList customThemeData{};
        customThemeData.reserve(static_cast<int>(customViewportColorThemes.size()));
        for ( const render::ViewportColorTheme& theme : customViewportColorThemes )
        {
            customThemeData.push_back(serializeTheme(theme));
        }
        themeCombo->setProperty("sed.customThemeData", customThemeData);
        const auto findThemeById = [themeCombo, deserializeTheme](const QString& id)
        {
            for ( const QVariant& item : themeCombo->property(
                     "sed.customThemeData").toList() )
            {
                const render::ViewportColorTheme theme =
                    deserializeTheme(item.toMap());
                if ( theme.id.compare(id, Qt::CaseInsensitive) == 0 )
                {
                    return theme;
                }
            }
            for ( const render::ViewportColorTheme& theme :
                 render::builtInViewportColorThemes() )
            {
                if ( theme.id.compare(id, Qt::CaseInsensitive) == 0 )
                {
                    return theme;
                }
            }
            const auto builtIns = render::builtInViewportColorThemes();
            return builtIns.front();
        };
        const auto rebuildThemeCombo = [themeCombo, deserializeTheme](
            const QString& selectedId)
        {
            const QSignalBlocker blocker(themeCombo);
            themeCombo->clear();
            int selectedIndex = 0;
            int index = 0;
            for ( const render::ViewportColorTheme& theme :
                 render::builtInViewportColorThemes() )
            {
                themeCombo->addItem(theme.label, theme.id);
                if ( theme.id.compare(selectedId, Qt::CaseInsensitive) == 0 )
                {
                    selectedIndex = index;
                }
                ++index;
            }
            for ( const QVariant& item : themeCombo->property(
                     "sed.customThemeData").toList() )
            {
                const render::ViewportColorTheme theme = deserializeTheme(item.toMap());
                if ( theme.id.isEmpty() || theme.label.isEmpty() )
                {
                    continue;
                }
                themeCombo->addItem(theme.label, theme.id);
                if ( theme.id.compare(selectedId, Qt::CaseInsensitive) == 0 )
                {
                    selectedIndex = index;
                }
                ++index;
            }
            themeCombo->setCurrentIndex(selectedIndex);
        };
        themeCombo->setToolTip(
            "Choose a built-in or saved theme. Add New copies the selected theme; built-in themes stay unchanged.");
        rebuildThemeCombo(viewportColorTheme);
        themeForm->addRow("Theme", themeCombo);
        auto* themeButtons = new QWidget(themeGroup);
        auto* themeButtonsLayout = new QHBoxLayout(themeButtons);
        themeButtonsLayout->setContentsMargins(0, 0, 0, 0);
        themeButtonsLayout->setSpacing(6);
        auto* saveThemeButton = new QPushButton("Add New...", themeButtons);
        saveThemeButton->setObjectName("pref_viewport_theme_add_new");
        auto* updateThemeButton = new QPushButton("Update Theme", themeButtons);
        updateThemeButton->setObjectName("pref_viewport_theme_update");
        auto* deleteThemeButton = new QPushButton("Delete Theme", themeButtons);
        deleteThemeButton->setObjectName("pref_viewport_theme_delete");
        saveThemeButton->setToolTip("Copy the selected theme into a new user theme that can be edited and saved.");
        updateThemeButton->setToolTip("Save the current color edits to the selected user theme.");
        deleteThemeButton->setToolTip("Delete the selected user theme.");
        themeButtonsLayout->addWidget(saveThemeButton);
        themeButtonsLayout->addWidget(updateThemeButton);
        themeButtonsLayout->addWidget(deleteThemeButton);
        themeButtonsLayout->addStretch(1);
        themeLayout->addLayout(themeForm);
        themeLayout->addWidget(themeButtons);
        const auto syncCustomThemeData = [&]()
        {
            QVariantList values{};
            values.reserve(static_cast<int>(customViewportColorThemes.size()));
            for ( const render::ViewportColorTheme& theme : customViewportColorThemes )
            {
                values.push_back(serializeTheme(theme));
            }
            themeCombo->setProperty("sed.customThemeData", values);
        };
        const auto refreshThemeButtons = [themeCombo, updateThemeButton,
            deleteThemeButton, &viewportThemeSwatches]()
        {
            const bool selectedCustom = themeCombo->currentData().toString()
                .startsWith(QStringLiteral("custom_"), Qt::CaseInsensitive);
            updateThemeButton->setEnabled(selectedCustom);
            deleteThemeButton->setEnabled(selectedCustom);
            for ( const auto& [button, color] : viewportThemeSwatches )
            {
                Q_UNUSED(color);
                if ( button )
                {
                    button->setEnabled(selectedCustom);
                    button->setToolTip(selectedCustom
                        ? button->property("sed.colorValue").value<QColor>()
                            .name(QColor::HexRgb)
                        : QStringLiteral("Duplicate this built-in theme with Add New before editing its colors."));
                }
            }
        };
        const auto stageSelectedThemeUpdate = [&]()
        {
            const QString currentId = themeCombo->currentData().toString();
            if ( !currentId.startsWith(QStringLiteral("custom_"),
                    Qt::CaseInsensitive) )
            {
                return;
            }
            for ( render::ViewportColorTheme& theme : customViewportColorThemes )
            {
                if ( theme.id.compare(currentId, Qt::CaseInsensitive) == 0 )
                {
                    theme.palette = captureViewportPalette();
                    syncCustomThemeData();
                    return;
                }
            }
        };
        persistViewportThemeFiles = [this, &customViewportColorThemes](QString* error)
        {
            QSettings settings = settingsStore();
            return render::saveCustomViewportColorThemes(
                settings, customViewportColorThemes, error);
        };
        const auto persistViewportThemeMutation = [&]()
        {
            QString error{};
            const bool persisted = persistViewportThemeFiles(&error);
            if (persisted)
            {
                originalViewportThemeFiles = customViewportColorThemes;
                viewportThemeFilesDirty = false;
                return true;
            }
            QMessageBox::warning(
                &dialog,
                QStringLiteral("Could Not Save Viewport Theme"),
                error.isEmpty() ? QStringLiteral("The viewport theme could not be saved.") : error);
            return false;
        };

        auto* appearanceGroup = new QGroupBox("Appearance", viewportColorsSection);
        auto* appearanceLayout = new QVBoxLayout(appearanceGroup);
        appearanceLayout->setContentsMargins(8, 8, 8, 8);
        auto* appearanceForm = new QFormLayout();
        appearanceForm->setContentsMargins(0, 0, 0, 0);
        appearanceForm->setSpacing(6);

        auto updateSwatch = [](QToolButton* button, const QColor& color)
        {
            if ( !button )
            {
                return;
            }
            button->setToolTip(color.name(QColor::HexRgb));
            button->setProperty("sed.colorValue", color);
            button->setStyleSheet(QString(
                "QToolButton {"
                "  background: %1;"
                "  border: 1px solid rgba(0, 0, 0, 160);"
                "  border-radius: 4px;"
                "}"
                "QToolButton:hover {"
                "  border-color: rgba(255, 255, 255, 140);"
                "}").arg(color.name(QColor::HexRgb)));
        };
        auto refreshThemeSwatches = [&viewportThemeSwatches, updateSwatch]()
        {
            for ( const auto& [button, color] : viewportThemeSwatches )
            {
                updateSwatch(button, *color);
            }
        };
        auto addColorRow = [&](QFormLayout* form,
                               QWidget* owner,
                               const QString& label,
                               QColor& value,
                               const QString& dialogTitle,
                               const QString& objectName,
                               const bool themeColor)
        {
            auto* swatch = new QToolButton(owner);
            swatch->setObjectName(objectName);
            swatch->setAutoRaise(true);
            swatch->setCursor(Qt::PointingHandCursor);
            swatch->setFixedSize(46, 18);
            updateSwatch(swatch, value);
            if ( themeColor )
            {
                viewportThemeSwatches.emplace_back(swatch, &value);
            }

            QObject::connect(
                swatch,
                &QToolButton::clicked,
                &dialog,
                [&, target = &value, swatch, dialogTitle, themeColor,
                    refreshThemeSwatches, updateSwatch, refreshThemeButtons]()
                {
                    const QColor chosen = QColorDialog::getColor(
                        *target, &dialog, dialogTitle);
                    if ( !chosen.isValid() )
                    {
                        return;
                    }
                    *target = chosen;
                    if ( themeColor )
                    {
                        // Named themes are edited as drafts. Update Theme
                        // applies the draft to the selected custom theme; Add
                        // New can copy the draft to another named theme.
                        viewportThemeDraftPending = true;
                        refreshThemeSwatches();
                        refreshThemeButtons();
                    }
                    else
                    {
                        updateSwatch(swatch, *target);
                    }
                });

            form->addRow(label, swatch);
        };

        addColorRow(themeForm, themeGroup, "Background", viewportBgClear,
            "Select background clear color", "pref_viewport_bg_clear", true);
        addColorRow(themeForm, themeGroup, "Wireframe", viewportWireColor,
            "Select wireframe color", "pref_viewport_wireframe", true);
        addColorRow(themeForm, themeGroup, "Grid minor", viewportGridColor,
            "Select minor grid-line color", "pref_viewport_grid_color", true);
        addColorRow(themeForm, themeGroup, "Grid major", viewportGridMajorColor,
            "Select major grid-line color", "pref_viewport_grid_major_color", true);
        addColorRow(themeForm, themeGroup, "Grid axis X", viewportGridAxisXColor,
            "Select X grid-axis color", "pref_viewport_grid_axis_x_color", true);
        addColorRow(themeForm, themeGroup, "Grid axis Y", viewportGridAxisYColor,
            "Select Y grid-axis color", "pref_viewport_grid_axis_y_color", true);
        addColorRow(themeForm, themeGroup, "Grid axis Z", viewportGridAxisZColor,
            "Select Z grid-axis color", "pref_viewport_grid_axis_z_color", true);
        addColorRow(themeForm, themeGroup, "Selection (primary)", viewportSelPrimary,
            "Select primary selection color", "pref_viewport_sel_primary", true);
        addColorRow(themeForm, themeGroup, "Selection (multi)", viewportSelMulti,
            "Select multi-selection color", "pref_viewport_sel_multi", true);
        addColorRow(themeForm, themeGroup, "Selection (edge multi)", viewportSelEdgeMulti,
            "Select secondary Edge selection color", "pref_viewport_sel_edge_multi", true);
        addColorRow(themeForm, themeGroup, "Selection (active)", viewportSelActive,
            "Select active selection color", "pref_viewport_sel_active", true);
        addColorRow(themeForm, themeGroup, "Selection (vertex)", viewportSelVertex,
            "Select vertex selection color", "pref_viewport_sel_vertex", true);
        addColorRow(themeForm, themeGroup, "Selection (vertex multi)", viewportSelVertexMulti,
            "Select multiple-vertex selection color", "pref_viewport_sel_vertex_multi", true);
        addColorRow(themeForm, themeGroup, "COG pick (hover)", viewportCogPickPrimary,
            "Select COG entity pick highlight color", "pref_viewport_cog_pick_primary", true);
        addColorRow(themeForm, themeGroup, "COG pick (active)", viewportCogPickActive,
            "Select COG entity active pick color", "pref_viewport_cog_pick_active", true);
        addColorRow(themeForm, themeGroup, "UV canvas background", uvCanvasBackgroundColor,
            "Select UV canvas background color", "pref_uv_canvas_background_color", true);
        addColorRow(themeForm, themeGroup, "UV grid main", uvGridMainColor,
            "Select main UV grid-line color", "pref_uv_grid_main_color", true);
        addColorRow(themeForm, themeGroup, "UV grid sub", uvGridSubColor,
            "Select subordinate UV grid-line color", "pref_uv_grid_sub_color", true);
        addColorRow(themeForm, themeGroup, "UV grid axis U", uvGridAxisUColor,
            "Select U-axis grid color", "pref_uv_grid_axis_u_color", true);
        addColorRow(themeForm, themeGroup, "UV grid axis V", uvGridAxisVColor,
            "Select V-axis grid color", "pref_uv_grid_axis_v_color", true);

        QObject::connect(
            themeCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            &dialog,
            [&, themeCombo, applyViewportPalette,
                findThemeById, refreshThemeSwatches, refreshThemeButtons](
                    const int index)
            {
                if ( index < 0 )
                {
                    return;
                }
                const QString selectedTheme = themeCombo->itemData(index).toString();
                if ( selectedTheme == viewportColorTheme )
                {
                    refreshThemeButtons();
                    return;
                }
                viewportThemeDraftPending = false;
                viewportColorTheme = selectedTheme;
                applyViewportPalette(findThemeById(selectedTheme).palette);
                refreshThemeSwatches();
                refreshThemeButtons();
            });

        QObject::connect(saveThemeButton, &QPushButton::clicked, &dialog,
            [&, captureViewportPalette, syncCustomThemeData,
                rebuildThemeCombo, refreshThemeSwatches, refreshThemeButtons]()
            {
                ThemeNameInputDialog nameDialog(&dialog);
                nameDialog.setObjectName(
                    QStringLiteral("pref_viewport_theme_name_dialog"));
                nameDialog.setWindowTitle(QStringLiteral("Add Viewport Theme"));
                nameDialog.setLabelText(
                    QStringLiteral("Name for the copied theme"));
                nameDialog.setWindowFlag(Qt::WindowMinimizeButtonHint, false);
                nameDialog.setWindowFlag(Qt::WindowMaximizeButtonHint, false);
                nameDialog.setWindowFlag(Qt::WindowMinMaxButtonsHint, false);
                nameDialog.setMinimumSize(620, 180);
                nameDialog.resize(620, 180);
                const int nameDialogResult = nameDialog.exec();
                if (nameDialogResult != QDialog::Accepted)
                {
                    return;
                }
                const QString label = nameDialog.textValue().trimmed();
                if (label.isEmpty())
                {
                    return;
                }
                if ( label.size() > 128 )
                {
                    QMessageBox::warning(
                        &dialog,
                        QStringLiteral("Theme Name Is Too Long"),
                        QStringLiteral("Theme names must be 128 characters or fewer."));
                    return;
                }
                const auto sameName = [&](const QString& existing)
                { return ui::theme_storage::themeNamesCollide(existing, label); };
                const auto builtInThemes = render::builtInViewportColorThemes();
                const bool collidesWithBuiltIn = std::any_of(
                    builtInThemes.begin(), builtInThemes.end(),
                    [&](const render::ViewportColorTheme& theme)
                    { return sameName(theme.label); });
                const bool collidesWithUser = std::any_of(
                    customViewportColorThemes.begin(), customViewportColorThemes.end(),
                    [&](const render::ViewportColorTheme& theme)
                    { return sameName(theme.label); });
                if (collidesWithBuiltIn || collidesWithUser)
                {
                    QMessageBox::warning(
                        &dialog,
                        QStringLiteral("Theme Name Already Exists"),
                        QStringLiteral("A viewport theme with this name already exists."));
                    return;
                }
                const QString id = QStringLiteral("custom_%1").arg(
                    ui::theme_storage::themeSlugFromLabel(label));
                const std::vector<render::ViewportColorTheme> previousThemes =
                    customViewportColorThemes;
                customViewportColorThemes.push_back({id, label, captureViewportPalette()});
                if (!persistViewportThemeMutation())
                {
                    customViewportColorThemes = previousThemes;
                    syncCustomThemeData();
                    rebuildThemeCombo(viewportColorTheme);
                    return;
                }
                viewportThemeDraftPending = false;
                syncCustomThemeData();
                viewportColorTheme = id;
                rebuildThemeCombo(id);
                refreshThemeSwatches();
                refreshThemeButtons();
            });
        QObject::connect(updateThemeButton, &QPushButton::clicked, &dialog,
            [&, stageSelectedThemeUpdate, refreshThemeButtons, findThemeById,
                applyViewportPalette, refreshThemeSwatches]()
            {
                const std::vector<render::ViewportColorTheme> previousThemes =
                    customViewportColorThemes;
                stageSelectedThemeUpdate();
                if (!persistViewportThemeMutation())
                {
                    customViewportColorThemes = previousThemes;
                    syncCustomThemeData();
                    rebuildThemeCombo(viewportColorTheme);
                    applyViewportPalette(findThemeById(viewportColorTheme).palette);
                    refreshThemeSwatches();
                    refreshThemeButtons();
                    return;
                }
                viewportThemeDraftPending = false;
                refreshThemeButtons();
            });
        QObject::connect(deleteThemeButton, &QPushButton::clicked, &dialog,
            [&, syncCustomThemeData, rebuildThemeCombo, findThemeById,
                applyViewportPalette, refreshThemeSwatches, refreshThemeButtons]()
            {
                const QString id = themeCombo->currentData().toString();
                if ( !id.startsWith(QStringLiteral("custom_"), Qt::CaseInsensitive) )
                {
                    return;
                }
                const auto found = std::find_if(customViewportColorThemes.begin(),
                    customViewportColorThemes.end(), [&](const auto& item)
                    { return item.id.compare(id, Qt::CaseInsensitive) == 0; });
                if ( found == customViewportColorThemes.end() ) return;
                const std::vector<render::ViewportColorTheme> previousThemes =
                    customViewportColorThemes;
                customViewportColorThemes.erase(found);
                if (!persistViewportThemeMutation())
                {
                    customViewportColorThemes = previousThemes;
                    syncCustomThemeData();
                    rebuildThemeCombo(id);
                    return;
                }
                syncCustomThemeData();
                QSettings settings = settingsStore();
                QString removeError{};
                const bool removedEverywhere =
                    ui::theme_storage::removeThemeDocumentByIdFromDirectories(
                        ui::theme_storage::directoriesForSettings(
                            settings, sed::AppThemeKind::viewport),
                        id, ui::theme_storage::ThemeFormat::viewport,
                        &removeError);
                if (!removedEverywhere)
                {
                    QMessageBox::warning(
                        &dialog,
                        QStringLiteral("Could Not Fully Delete Viewport Theme"),
                        removeError.isEmpty()
                            ? QStringLiteral("A viewport theme file could not be removed.")
                            : removeError);
                }
                viewportThemeDraftPending = false;
                viewportColorTheme = QString::fromLatin1(render::kViewportColorThemeNightBlue);
                settings.setValue(QString::fromLatin1(render::kViewportColorThemeSetting),
                    viewportColorTheme);
                settings.sync();
                rebuildThemeCombo(viewportColorTheme);
                applyViewportPalette(findThemeById(viewportColorTheme).palette);
                refreshThemeSwatches();
                refreshThemeButtons();
            });
        refreshThemeButtons();

        auto* wireLineWidthSpin = new QDoubleSpinBox(appearanceGroup);
        wireLineWidthSpin->setObjectName("pref_viewport_wire_line_width");
        wireLineWidthSpin->setRange(0.5, 6.0);
        wireLineWidthSpin->setDecimals(2);
        wireLineWidthSpin->setSingleStep(0.1);
        wireLineWidthSpin->setSuffix(" px");
        wireLineWidthSpin->setValue(std::clamp(viewportWireLineWidthPx, 0.5, 6.0));
        wireLineWidthSpin->setToolTip("Line width for world wireframe rendering.");
        appearanceForm->addRow("Wireframe width", wireLineWidthSpin);

        auto* gridLineWidthSpin = new QDoubleSpinBox(appearanceGroup);
        gridLineWidthSpin->setObjectName("pref_viewport_grid_line_width");
        gridLineWidthSpin->setRange(
            static_cast<double>(render::kGridLineWidthMinPx),
            static_cast<double>(render::kGridLineWidthMaxPx));
        gridLineWidthSpin->setDecimals(2);
        gridLineWidthSpin->setSingleStep(0.25);
        gridLineWidthSpin->setSuffix(" px");
        gridLineWidthSpin->setValue(static_cast<double>(
            render::clampedGridLineWidthPx(static_cast<float>(viewportGridLineWidthPx))));
        gridLineWidthSpin->setToolTip(
            "Pixel width of minor and major grid lines. Colored axes stay bounded for readability.");
        appearanceForm->addRow("Grid width", gridLineWidthSpin);

        auto* wireAntialiasingCheck = new QCheckBox("Enable", appearanceGroup);
        wireAntialiasingCheck->setObjectName("pref_viewport_wire_aa");
        wireAntialiasingCheck->setChecked(viewportWireAntialiasing);
        wireAntialiasingCheck->setToolTip("Toggle wireframe anti-aliasing (MSAA + smooth fallback).");
        appearanceForm->addRow("Wireframe AA", wireAntialiasingCheck);

        auto* msaaSamplesCombo = new QComboBox(appearanceGroup);
        msaaSamplesCombo->setObjectName("pref_viewport_msaa_samples");
        for ( const int samples : { 0, 2, 4, 8, 16 } )
        {
            msaaSamplesCombo->addItem(
                samples == 0 ? QStringLiteral("Off")
                             : QStringLiteral("%1x").arg(samples),
                samples);
        }
        msaaSamplesCombo->setCurrentIndex(
            std::max(0, msaaSamplesCombo->findData(viewportMsaaSamples)));
        msaaSamplesCombo->setToolTip(
            "Requested scene MSAA samples. The graphics driver may provide fewer samples. Restart SED to apply this setting.");
        appearanceForm->addRow("MSAA samples (restart required)", msaaSamplesCombo);

        auto* framePathLineSpin = new QDoubleSpinBox(appearanceGroup);
        framePathLineSpin->setObjectName("pref_viewport_frame_path_line_world_size");
        framePathLineSpin->setRange(0.001, 1.0);
        framePathLineSpin->setDecimals(4);
        framePathLineSpin->setSingleStep(0.005);
        framePathLineSpin->setSuffix(" units");
        framePathLineSpin->setValue(std::clamp(viewportFramePathLineWorldSize, 0.001, 1.0));
        framePathLineSpin->setToolTip("World-space thickness used for thing frame-path lines.");
        appearanceForm->addRow("Frame path line", framePathLineSpin);

        auto* framePathPointSpin = new QDoubleSpinBox(appearanceGroup);
        framePathPointSpin->setObjectName("pref_viewport_frame_path_point_world_size");
        framePathPointSpin->setRange(0.005, 2.0);
        framePathPointSpin->setDecimals(4);
        framePathPointSpin->setSingleStep(0.01);
        framePathPointSpin->setSuffix(" units");
        framePathPointSpin->setValue(std::clamp(viewportFramePathPointWorldSize, 0.005, 2.0));
        framePathPointSpin->setToolTip("World-space radius used for thing frame-path points.");
        appearanceForm->addRow("Frame path point", framePathPointSpin);

        auto* wireLayerCheck = new QCheckBox("Use layer colors", appearanceGroup);
        wireLayerCheck->setObjectName("pref_layer_wireframe_colors");
        wireLayerCheck->setChecked(viewportWireLayerColors);
        appearanceForm->addRow("Wireframe by layer", wireLayerCheck);

        auto* outlinerLayerCheck = new QCheckBox("Use layer colors", appearanceGroup);
        outlinerLayerCheck->setObjectName("pref_layer_outliner_colors");
        outlinerLayerCheck->setChecked(viewportOutlinerLayerColors);
        appearanceForm->addRow("Outliner by layer", outlinerLayerCheck);

        appearanceLayout->addLayout(appearanceForm);
        viewportColorsSection->contentLayout()->addWidget(themeGroup);
        viewportColorsSection->contentLayout()->addWidget(appearanceGroup);
    }
    viewportLayout->insertWidget(0, viewportColorsSection);

    auto* const bannerSection = createCollapsedSection(
        QStringLiteral("banners"), "Banners", viewportPage);
    bannerSection->setObjectName(QStringLiteral("pref_viewport_banners"));
    auto* const bannerForm = new QGridLayout();
    bannerForm->setContentsMargins(6, 4, 6, 4);
    bannerForm->setHorizontalSpacing(8);
    bannerForm->setVerticalSpacing(6);
    bannerForm->addWidget(new QLabel(QStringLiteral("Hide after"), bannerSection), 0, 1);
    bannerForm->addWidget(new QLabel(QStringLiteral("Text"), bannerSection), 0, 2);
    bannerForm->addWidget(new QLabel(QStringLiteral("Background"), bannerSection), 0, 3);
    bannerForm->addWidget(new QLabel(QStringLiteral("Border"), bannerSection), 0, 4);
    bannerForm->addWidget(new QLabel(QStringLiteral("Duration source"), bannerSection), 0, 5);

    const auto updateBannerSwatch = [](QToolButton* const button, const QColor& color)
    {
        if ( !button )
        {
            return;
        }
        button->setProperty("sed.colorValue", color);
        button->setToolTip(color.name(QColor::HexArgb));
        button->setStyleSheet(QStringLiteral(
            "QToolButton { background-color: %1; border: 1px solid rgba(0, 0, 0, 160); "
            "border-radius: 5px; min-width: 42px; min-height: 20px; }"
            "QToolButton:hover { border-color: rgba(255, 255, 255, 150); }")
            .arg(color.name(QColor::HexArgb)));
    };

    const std::array<const char*, 3> bannerColorFieldIds{
        "text_color", "background_color", "border_color",
    };
    const std::array<const char*, 3> bannerColorLabels{
        "Text", "Background", "Border",
    };
    for ( std::size_t index = 0; index < kViewportBannerKindCount; ++index )
    {
        const auto kind = static_cast<ViewportBannerKind>(index);
        const QString id = QString::fromLatin1(kViewportBannerKindIds[index]);
        const QString label = QString::fromLatin1(kViewportBannerKindLabels[index]);
        const QString prefix = QStringLiteral("pref_banner_%1_").arg(id);
        const ViewportBannerStyle& originalStyle =
            originalViewportBannerSettings.styles[index];
        const int row = static_cast<int>(index + 1u);

        auto* const bannerLabel = new QLabel(label, bannerSection);
        bannerLabel->setObjectName(prefix + QStringLiteral("label"));
        bannerForm->addWidget(bannerLabel, row, 0);
        auto* const duration = new QDoubleSpinBox(bannerSection);
        duration->setObjectName(prefix + QStringLiteral("hide_time"));
        duration->setProperty("sed.viewportBannerPreference", true);
        duration->setRange(0.10, 30.00);
        duration->setDecimals(3);
        duration->setSingleStep(0.10);
        duration->setSuffix(QStringLiteral(" s"));
        duration->setValue(static_cast<double>(originalStyle.hideAfterMs) / 1000.0);
        duration->setToolTip(QStringLiteral("Hide this banner after the selected time."));
        duration->setMaximumWidth(112);
        bannerForm->addWidget(duration, row, 1);

        if ( kind == ViewportBannerKind::general || kind == ViewportBannerKind::error )
        {
            auto* const useMessageDuration = new QCheckBox(
                QStringLiteral("Use message duration"), bannerSection);
            useMessageDuration->setObjectName(prefix + QStringLiteral("use_message_duration"));
            useMessageDuration->setProperty("sed.viewportBannerPreference", true);
            useMessageDuration->setChecked(originalStyle.useMessageDuration);
            useMessageDuration->setToolTip(
                QStringLiteral("Keep each message's supplied duration. Clear this to use Hide after."));
            bannerForm->addWidget(useMessageDuration, row, 5, Qt::AlignLeft);
        }

        const std::array<QColor, 3> colors{
            originalStyle.textColor, originalStyle.backgroundColor,
            originalStyle.borderColor,
        };
        for ( std::size_t colorIndex = 0; colorIndex < bannerColorFieldIds.size(); ++colorIndex )
        {
            auto* const swatch = new QToolButton(bannerSection);
            swatch->setObjectName(prefix + QString::fromLatin1(bannerColorFieldIds[colorIndex]));
            swatch->setProperty("sed.viewportBannerPreference", true);
            swatch->setAutoRaise(true);
            swatch->setCursor(Qt::PointingHandCursor);
            swatch->setAccessibleName(QStringLiteral("%1 %2 color")
                .arg(label, QString::fromLatin1(bannerColorLabels[colorIndex])));
            updateBannerSwatch(swatch, colors[colorIndex]);
            QObject::connect(swatch, &QToolButton::clicked, &dialog,
                [swatch, label, colorLabel = QString::fromLatin1(bannerColorLabels[colorIndex]),
                    updateBannerSwatch, &dialog]()
                {
                    QColorDialog picker(
                        swatch->property("sed.colorValue").value<QColor>(), &dialog);
                    picker.setObjectName(QStringLiteral("pref_banner_color_dialog"));
                    picker.setWindowTitle(QStringLiteral("Select %1 %2 color")
                        .arg(label, colorLabel));
                    picker.setOption(QColorDialog::ShowAlphaChannel, true);
                    if ( picker.exec() == QDialog::Accepted )
                    {
                        updateBannerSwatch(swatch, picker.currentColor());
                    }
                });
            bannerForm->addWidget(swatch, row, static_cast<int>(colorIndex + 2u),
                Qt::AlignLeft);
        }
    }

    auto* const resetBanners = new QPushButton(
        QStringLiteral("Restore Defaults"), bannerSection);
    resetBanners->setObjectName(QStringLiteral("pref_banner_reset_defaults"));
    QObject::connect(resetBanners, &QPushButton::clicked, &dialog,
        [&dialog, updateBannerSwatch, bannerColorFieldIds]()
        {
            const ViewportBannerSettings defaults{};
            for ( std::size_t index = 0; index < kViewportBannerKindCount; ++index )
            {
                const QString prefix = QStringLiteral("pref_banner_%1_")
                    .arg(QString::fromLatin1(kViewportBannerKindIds[index]));
                if ( auto* const duration = dialog.findChild<QDoubleSpinBox*>(prefix + "hide_time") )
                {
                    duration->setValue(
                        static_cast<double>(defaults.styles[index].hideAfterMs) / 1000.0);
                }
                if ( auto* const useMessageDuration =
                    dialog.findChild<QCheckBox*>(prefix + "use_message_duration") )
                {
                    useMessageDuration->setChecked(
                        defaults.styles[index].useMessageDuration);
                }
                const std::array<QColor, 3> colors{
                    defaults.styles[index].textColor,
                    defaults.styles[index].backgroundColor,
                    defaults.styles[index].borderColor,
                };
                for ( std::size_t colorIndex = 0; colorIndex < bannerColorFieldIds.size(); ++colorIndex )
                {
                    const QString name = prefix + QString::fromLatin1(bannerColorFieldIds[colorIndex]);
                    updateBannerSwatch(dialog.findChild<QToolButton*>(name), colors[colorIndex]);
                }
            }
        });
    bannerForm->addWidget(resetBanners, static_cast<int>(kViewportBannerKindCount + 1u),
        0, 1, 6, Qt::AlignRight);
    bannerSection->contentLayout()->addLayout(bannerForm);
    viewportLayout->addWidget(bannerSection);

    // Grid section.
    auto* gridSection = createCollapsedSection(
        QStringLiteral("grid"), "Grid", viewportPage);
    {
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        QSettings settings = settingsStore();

        auto* showGridCheck = new QCheckBox("Show grid (Editor)", gridSection);
        showGridCheck->setObjectName("pref_viewport_show_grid");
        // Map Settings applies this preference immediately.  Read the live
        // viewport state when the Preferences page is opened so accepting an
        // unrelated preference (for example extrude-preview opacity) cannot
        // overwrite a recent map-settings toggle with a stale QSettings
        // value.  The persisted value remains the fallback for a page opened
        // before the viewport has been created.
        showGridCheck->setChecked(
            glView_ ? glView_->showGrid()
                    : settings.value("viewport/show_grid", true).toBool());
        form->addRow("Visibility", showGridCheck);

        auto* followGridCheck = new QCheckBox("Grid follows camera", gridSection);
        followGridCheck->setObjectName("pref_viewport_grid_follow_camera");
        followGridCheck->setToolTip("When enabled, the grid display follows the camera target as you pan.");
        followGridCheck->setChecked(settings.value("viewport/grid_follow_camera", true).toBool());
        followGridCheck->setEnabled(showGridCheck->isChecked());
        form->addRow("Center", followGridCheck);

        auto* rotateGridCheck = new QCheckBox("Rotate grid to view", gridSection);
        rotateGridCheck->setObjectName("pref_viewport_grid_rotate_with_view");
        rotateGridCheck->setToolTip("Auto-orient the grid plane based on the camera view direction.");
        rotateGridCheck->setChecked(settings.value("viewport/grid_rotate_with_view", false).toBool());
        rotateGridCheck->setEnabled(showGridCheck->isChecked());
        form->addRow("Orientation", rotateGridCheck);

        auto* keepViewGridVisibleCheck = new QCheckBox(
            "Keep rotated grid visible", gridSection);
        keepViewGridVisibleCheck->setObjectName(
            "pref_viewport_grid_keep_view_aligned_visible");
        keepViewGridVisibleCheck->setToolTip(
            "When the view-aligned grid crosses the camera, display a "
            "phase-matched guide just beyond the near plane. Snapping and "
            "the authored grid origin remain unchanged.");
        keepViewGridVisibleCheck->setChecked(
            glView_ ? glView_->gridKeepViewAlignedVisible()
                    : settings.value(
                        "viewport/grid_keep_view_aligned_visible", false)
                          .toBool());
        keepViewGridVisibleCheck->setEnabled(
            showGridCheck->isChecked() && rotateGridCheck->isChecked());
        form->addRow("Near plane", keepViewGridVisibleCheck);

        auto* gridMajorSpin = new QDoubleSpinBox(gridSection);
        gridMajorSpin->setObjectName("pref_viewport_grid_major_spacing");
        gridMajorSpin->setDecimals(6);
        gridMajorSpin->setRange(0.000001, 1000000.0);
        gridMajorSpin->setSingleStep(0.1);
        gridMajorSpin->setValue(settings.value("viewport/grid_spacing", 1.0).toDouble());
        gridMajorSpin->setEnabled(showGridCheck->isChecked());
        form->addRow("Major", gridMajorSpin);

        auto* gridMinorSpin = new QDoubleSpinBox(gridSection);
        gridMinorSpin->setObjectName("pref_viewport_grid_minor_spacing");
        gridMinorSpin->setDecimals(6);
        gridMinorSpin->setRange(0.000001, 1000000.0);
        gridMinorSpin->setSingleStep(0.01);
        gridMinorSpin->setValue(settings.value("viewport/grid_minor_spacing", 0.1).toDouble());
        gridMinorSpin->setEnabled(showGridCheck->isChecked());
        form->addRow("Minor", gridMinorSpin);

        auto* gridMinorStyleCombo = new QComboBox(gridSection);
        gridMinorStyleCombo->setObjectName("pref_viewport_grid_minor_style");
        gridMinorStyleCombo->addItem("Lines", false);
        gridMinorStyleCombo->addItem("Points", true);
        const bool gridMinorDots = glView_
            ? glView_->gridMinorDots()
            : settings.value("viewport/grid_minor_dots", false).toBool();
        gridMinorStyleCombo->setCurrentIndex(
            gridMinorStyleCombo->findData(gridMinorDots));
        gridMinorStyleCombo->setEnabled(showGridCheck->isChecked());
        form->addRow("Minor grid", gridMinorStyleCombo);

        auto* gridMinorDotSizeSpin = new QDoubleSpinBox(gridSection);
        gridMinorDotSizeSpin->setObjectName(
            "pref_viewport_grid_minor_dot_size");
        gridMinorDotSizeSpin->setRange(
            render::kGridMinorDotSizeMinPx,
            render::kGridMinorDotSizeMaxPx);
        gridMinorDotSizeSpin->setDecimals(2);
        gridMinorDotSizeSpin->setSingleStep(0.25);
        gridMinorDotSizeSpin->setSuffix(" px");
        gridMinorDotSizeSpin->setKeyboardTracking(false);
        gridMinorDotSizeSpin->setValue(glView_
            ? static_cast<double>(glView_->gridMinorDotSizePx())
            : settings.value("viewport/grid_minor_dot_size_px", 2.0)
                  .toDouble());
        gridMinorDotSizeSpin->setEnabled(
            showGridCheck->isChecked() && gridMinorDots);
        form->addRow("Dot size", gridMinorDotSizeSpin);

        auto* gridOpacitySpin = new QSpinBox(gridSection);
        gridOpacitySpin->setObjectName("pref_viewport_grid_opacity");
        gridOpacitySpin->setRange(0, 100);
        gridOpacitySpin->setSingleStep(5);
        gridOpacitySpin->setSuffix("%");
        gridOpacitySpin->setValue(qRound(
            std::clamp(settings.value("viewport/grid_opacity", 0.70).toDouble(), 0.0, 1.0) * 100.0));
        gridOpacitySpin->setToolTip(
            "Controls grid-line opacity over the viewport. Increase it when bright textures wash out the grid.");
        gridOpacitySpin->setEnabled(showGridCheck->isChecked());
        form->addRow("Opacity", gridOpacitySpin);

        auto* adaptiveGridCheck = new QCheckBox("Auto scale with zoom", gridSection);
        adaptiveGridCheck->setObjectName("pref_viewport_grid_adaptive_spacing");
        adaptiveGridCheck->setToolTip("Automatically chooses a readable spacing (as a multiple of the grid size) based on zoom level.");
        adaptiveGridCheck->setChecked(settings.value("viewport/grid_adaptive_spacing", false).toBool());
        adaptiveGridCheck->setEnabled(showGridCheck->isChecked());
        form->addRow("Sub-grid", adaptiveGridCheck);

        auto* uvMaximumZoomSpin = new QSpinBox(gridSection);
        uvMaximumZoomSpin->setObjectName("pref_uv_editor_max_zoom");
        uvMaximumZoomSpin->setRange(480, 480'000);
        uvMaximumZoomSpin->setSingleStep(480);
        uvMaximumZoomSpin->setSuffix(" px/UV");
        bool uvMaximumZoomOk = false;
        const int uvMaximumZoom = settings.value(
            "uv_editor/max_zoom", 48'000).toInt(&uvMaximumZoomOk);
        uvMaximumZoomSpin->setValue(uvMaximumZoomOk
            ? std::clamp(uvMaximumZoom, 480, 480'000) : 48'000);
        uvMaximumZoomSpin->setToolTip(
            "Maximum UV canvas zoom. The ordinary default remains 48 px per UV unit.");
        form->addRow("UV editor maximum zoom", uvMaximumZoomSpin);

        auto* gridVisibilityCombo = new QComboBox(gridSection);
        gridVisibilityCombo->setObjectName("pref_viewport_grid_visibility");
        gridVisibilityCombo->setToolTip(
            "Occluded is the default: solid level geometry and Thing objects hide grid fragments behind them.");
        gridVisibilityCombo->addItem("Occluded by solid geometry (default)", 0);
        gridVisibilityCombo->addItem("Visible through walls", 1);
        gridVisibilityCombo->addItem("Always behind geometry", 2);
        const bool gridAlwaysBehind = settings.value("viewport/grid_always_behind", false).toBool();
        const bool gridVisibleThroughWalls = !gridAlwaysBehind
            && settings.value("viewport/grid_visible_through_walls", false).toBool();
        gridVisibilityCombo->setCurrentIndex(gridAlwaysBehind ? 2 : (gridVisibleThroughWalls ? 1 : 0));
        gridVisibilityCombo->setEnabled(showGridCheck->isChecked());
        form->addRow("Visibility", gridVisibilityCombo);

        auto* resetGridOnRotateCheck = new QCheckBox("Reset align/center on camera rotate", gridSection);
        resetGridOnRotateCheck->setObjectName("pref_viewport_grid_reset_on_rotate");
        resetGridOnRotateCheck->setToolTip("When enabled, view-aligned/centered grid overrides reset as soon as the camera rotates.");
        resetGridOnRotateCheck->setChecked(settings.value("viewport/grid_reset_on_rotate", true).toBool());
        resetGridOnRotateCheck->setEnabled(showGridCheck->isChecked());
        form->addRow("Reset", resetGridOnRotateCheck);

        auto* transformGeometrySnapCheck = new QCheckBox("Snap dragged sectors/surfaces to nearby geometry", gridSection);
        transformGeometrySnapCheck->setObjectName("pref_viewport_transform_geometry_snap");
        transformGeometrySnapCheck->setChecked(settings.value("viewport/transform_geometry_snap", true).toBool());
        form->addRow("Transform snap", transformGeometrySnapCheck);

        auto* transformGeometryRotateCheck = new QCheckBox(
            "Rotate moved faces and freely moved edges to nearby orientation",
            gridSection);
        transformGeometryRotateCheck->setObjectName(
            "pref_viewport_transform_geometry_rotate");
        transformGeometryRotateCheck->setChecked(
            settings.value("viewport/transform_geometry_rotate", true).toBool());
        transformGeometryRotateCheck->setEnabled(
            transformGeometrySnapCheck->isChecked());
        transformGeometryRotateCheck->setToolTip(
            "Uses the closest parallel normal/direction after Nearby Snap. "
            "Face axis moves rotate too; opposed face normals remain the "
            "normal adjoining-sector result.");
        form->addRow("Transform rotate", transformGeometryRotateCheck);

        auto* preserveConnectedSurfaceUvsCheck =
            new QCheckBox("Preserve connected surface UVs during Move, Rotate, and Scale", gridSection);
        preserveConnectedSurfaceUvsCheck->setObjectName("pref_viewport_preserve_connected_surface_uvs");
        preserveConnectedSurfaceUvsCheck->setChecked(
            settings.value("viewport/preserve_connected_surface_uvs", true).toBool());
        preserveConnectedSurfaceUvsCheck->setToolTip("Keeps rigid face mapping unchanged and reprojects deformed "
                                                     "neighbor faces from their original texture basis. This is "
                                                     "independent of the legacy Scale textures option.");
        form->addRow("Transform UVs", preserveConnectedSurfaceUvsCheck);

        QObject::connect(showGridCheck, &QCheckBox::toggled, followGridCheck, &QWidget::setEnabled);
        QObject::connect(showGridCheck, &QCheckBox::toggled, rotateGridCheck, &QWidget::setEnabled);
        QObject::connect(
            showGridCheck, &QCheckBox::toggled, keepViewGridVisibleCheck,
            [rotateGridCheck, keepViewGridVisibleCheck](const bool shown)
            {
                keepViewGridVisibleCheck->setEnabled(
                    shown && rotateGridCheck->isChecked());
            });
        QObject::connect(
            rotateGridCheck, &QCheckBox::toggled, keepViewGridVisibleCheck,
            [showGridCheck, keepViewGridVisibleCheck](const bool rotated)
            {
                keepViewGridVisibleCheck->setEnabled(
                    rotated && showGridCheck->isChecked());
            });
        QObject::connect(showGridCheck, &QCheckBox::toggled, gridMajorSpin, &QWidget::setEnabled);
        QObject::connect(showGridCheck, &QCheckBox::toggled, gridMinorSpin, &QWidget::setEnabled);
        QObject::connect(showGridCheck, &QCheckBox::toggled,
                         gridMinorStyleCombo, &QWidget::setEnabled);
        QObject::connect(
            showGridCheck, &QCheckBox::toggled, gridMinorDotSizeSpin,
            [gridMinorStyleCombo, gridMinorDotSizeSpin](const bool shown)
            {
                gridMinorDotSizeSpin->setEnabled(
                    shown && gridMinorStyleCombo->currentData().toBool());
            });
        QObject::connect(
            gridMinorStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            gridMinorDotSizeSpin,
            [showGridCheck, gridMinorStyleCombo, gridMinorDotSizeSpin](const int)
            {
                gridMinorDotSizeSpin->setEnabled(
                    showGridCheck->isChecked()
                    && gridMinorStyleCombo->currentData().toBool());
            });
        QObject::connect(showGridCheck, &QCheckBox::toggled, gridOpacitySpin, &QWidget::setEnabled);
        QObject::connect(showGridCheck, &QCheckBox::toggled, adaptiveGridCheck, &QWidget::setEnabled);
        QObject::connect(showGridCheck, &QCheckBox::toggled, gridVisibilityCombo, &QWidget::setEnabled);
        QObject::connect(showGridCheck, &QCheckBox::toggled, resetGridOnRotateCheck, &QWidget::setEnabled);
        QObject::connect(
            transformGeometrySnapCheck,
            &QCheckBox::toggled,
            transformGeometryRotateCheck,
            &QWidget::setEnabled);

        gridSection->contentLayout()->addLayout(form);
    }

    viewportLayout->addWidget(gridSection);

    // Snapping section. Distinct radii mirror the distinct designer actions;
    // changing one tool's feel does not unexpectedly change another.
    auto* snappingSection = createCollapsedSection(
        QStringLiteral("snapping"), "Snapping", viewportPage);
    {
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);
        QSettings settings = settingsStore();

        const auto addRadius = [form, snappingSection, &settings](
                                   const char* label,
                                   const char* objectName,
                                   const char* settingKey,
                                   const double defaultValue,
                                   const char* tooltip)
        {
            auto* spin = new QDoubleSpinBox(snappingSection);
            spin->setObjectName(QString::fromLatin1(objectName));
            spin->setRange(0.000001, 1000.0);
            spin->setDecimals(6);
            spin->setSingleStep(0.01);
            spin->setSuffix(" units");
            spin->setValue(settings.value(
                QString::fromLatin1(settingKey), defaultValue).toDouble());
            spin->setToolTip(QString::fromUtf8(tooltip));
            form->addRow(QString::fromUtf8(label), spin);
        };

        addRadius("Vertex to vertex", "pref_snap_radius_vertex",
                  "viewport/snap_radius_vertex", 0.20,
                  "How close a moved vertex must be to another vertex before Nearby Snap captures it.");
        addRadius("Edge to edge", "pref_snap_radius_edge",
                  "viewport/snap_radius_edge", 0.20,
                  "How close a moved edge must be to a same-oriented edge before Nearby Snap captures it.");
        addRadius("Face to face", "pref_snap_radius_face",
                  "viewport/snap_radius_face", 2.0,
                  "Search distance used when a moved face looks for a suitable nearby face.");
        addRadius("Sector to sector", "pref_snap_radius_sector",
                  "viewport/snap_radius_sector", 2.0,
                  "Search distance used when a moved sector aligns with nearby level geometry.");
        addRadius("Thing to Thing", "pref_snap_radius_thing",
                  "viewport/snap_radius_thing", 0.16,
                  "Base Thing snapping distance. Placement and connector tools scale this base for their larger probes; rail connectors use the widest search.");
        addRadius("Surface / wall stick", "pref_surface_stick_radius",
                  "viewport/surface_stick_radius", 0.22,
                  "Maximum distance at which a moved Thing sticks to a surface or wall.");
        addRadius("Grid-plane stick", "pref_grid_plane_stick_radius",
                  "viewport/grid_plane_stick_radius", 0.08,
                  "Distance from the active grid plane at which a moved item is captured by Grid Stick.");

        auto* rotation = new QDoubleSpinBox(snappingSection);
        rotation->setObjectName("pref_snap_rotation_max_degrees");
        rotation->setRange(0.0, 180.0);
        rotation->setDecimals(2);
        rotation->setSingleStep(1.0);
        rotation->setSuffix(QString::fromUtf8("°"));
        rotation->setValue(settings.value(
            "viewport/snap_rotation_max_degrees", 180.0).toDouble());
        rotation->setToolTip(
            "Largest automatic rotation allowed while snapping. Edge and "
            "sector moves keep their additional conservative limits; 0° "
            "allows translation-only matches.");
        form->addRow("Snap rotation", rotation);

        snappingSection->contentLayout()->addLayout(form);
    }
    viewportLayout->addWidget(snappingSection);

    const ViewportNavigationSettings originalMouseNavigation = glView_
        ? glView_->viewportNavigationSettings() : loadViewportNavigationSettings(settingsStore());
    std::array<QComboBox*, 6> mouseButtons{};
    std::array<QComboBox*, 6> mouseModifiers{};
    std::array<QComboBox*, 2> wheelModifiers{};
    std::array<QCheckBox*, 2> wheelInversion{};
    auto* const mouseNavigationSection = createCollapsedSection(
        QStringLiteral("mouse-navigation"), "Mouse Controls", viewportPage);
    auto* const mouseForm = new QGridLayout();
    mouseForm->setContentsMargins(6, 4, 6, 4);
    mouseForm->setSpacing(6);
    mouseForm->setColumnStretch(1, 1);
    mouseForm->setColumnStretch(2, 1);
    auto* const buttonHeading = new QLabel("Mouse button", mouseNavigationSection);
    buttonHeading->setObjectName("pref_mouse_button_heading");
    auto* const modifierHeading = new QLabel("Keyboard modifiers", mouseNavigationSection);
    modifierHeading->setObjectName("pref_mouse_modifiers_heading");
    mouseForm->addWidget(buttonHeading, 0, 1);
    mouseForm->addWidget(modifierHeading, 0, 2);
    const auto populateMouseModifiers = [](QComboBox& combo, const bool wheel)
    {
        if ( wheel )
        {
            combo.addItem("Disabled", -1);
        }
        const std::array<Qt::KeyboardModifier, 4> flags{
            Qt::ShiftModifier, Qt::ControlModifier, Qt::AltModifier, Qt::MetaModifier,
        };
        for ( unsigned int bits = 0; bits < 16u; ++bits )
        {
            Qt::KeyboardModifiers modifiers{};
            for ( std::size_t flag = 0; flag < flags.size(); ++flag )
            {
                if ( (bits & (1u << flag)) != 0u )
                {
                    modifiers |= flags[flag];
                }
            }
            combo.addItem(modifiers == Qt::NoModifier ? QStringLiteral("No modifiers")
                : ui::shortcuts::modifierText(modifiers), int(modifiers));
        }
        combo.setProperty("sed.mouseNavigationPreference", true);
    };
    for ( std::size_t index = 0; index < mouseButtons.size(); ++index )
    {
        const int row = static_cast<int>(index + 1);
        auto* const button = new QComboBox(mouseNavigationSection);
        button->setObjectName(QStringLiteral("pref_mouse_%1_button")
            .arg(QString::fromLatin1(kViewportMouseActionIds[index])));
        button->setProperty("sed.mouseNavigationPreference", true);
        for ( const auto& [label, value] : std::array{
            std::pair{"Disabled", Qt::NoButton}, std::pair{"Left button", Qt::LeftButton},
            std::pair{"Right button", Qt::RightButton}, std::pair{"Middle button (wheel click)", Qt::MiddleButton},
            std::pair{"Back button", Qt::BackButton}, std::pair{"Forward button", Qt::ForwardButton} } )
        {
            button->addItem(QString::fromLatin1(label), int(value));
        }
        button->setCurrentIndex(button->findData(int(originalMouseNavigation.mouse[index].button)));
        auto* const modifiers = new QComboBox(mouseNavigationSection);
        modifiers->setObjectName(QStringLiteral("pref_mouse_%1_modifiers")
            .arg(QString::fromLatin1(kViewportMouseActionIds[index])));
        populateMouseModifiers(*modifiers, false);
        modifiers->setCurrentIndex(modifiers->findData(int(originalMouseNavigation.mouse[index].modifiers)));
        modifiers->setEnabled(originalMouseNavigation.mouse[index].button != Qt::NoButton);
        button->setToolTip("The physical button that starts this viewport action. Middle button means pressing the wheel; turning it is configured under Wheel scrolling. Release that same button to end its gesture. Disabled never captures mouse motion.");
        modifiers->setToolTip("No modifiers requires no activation key. Only modifiers reserved by actions on this button select its action; other modifiers retain their selection, snapping, and Fly speed behavior.");
        mouseForm->addWidget(new QLabel(QString::fromLatin1(kViewportMouseActionLabels[index]), mouseNavigationSection), row, 0);
        mouseForm->addWidget(button, row, 1);
        mouseForm->addWidget(modifiers, row, 2);
        mouseButtons[index] = button;
        mouseModifiers[index] = modifiers;
    }
    auto* const wheelSection = new QWidget(mouseNavigationSection);
    wheelSection->setObjectName("pref_mouse_wheel_section");
    auto* const wheelForm = new QGridLayout(wheelSection);
    wheelForm->setContentsMargins(0, 6, 0, 0);
    wheelForm->setSpacing(6);
    wheelForm->setColumnStretch(3, 1);
    auto* const wheelHeading = new QLabel("Wheel scrolling", wheelSection);
    wheelHeading->setObjectName("pref_mouse_wheel_heading");
    wheelHeading->setToolTip("Turn the wheel to scroll. Pressing it is the separate Middle button (wheel click) choice above.");
    wheelForm->addWidget(wheelHeading, 0, 0);
    wheelForm->addWidget(new QLabel("Keyboard modifiers", wheelSection), 0, 1);
    const std::array<const char*, 2> wheelIds{ "zoom", "fly_speed" };
    for ( std::size_t index = 0; index < wheelModifiers.size(); ++index )
    {
        const int row = static_cast<int>(index + 1);
        const ViewportWheelBinding& binding = index == 0u
            ? originalMouseNavigation.zoom : originalMouseNavigation.flySpeed;
        auto* const modifiers = new QComboBox(wheelSection);
        modifiers->setObjectName(QStringLiteral("pref_mouse_%1_modifiers")
            .arg(QString::fromLatin1(wheelIds[index])));
        populateMouseModifiers(*modifiers, true);
        modifiers->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        modifiers->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        modifiers->setCurrentIndex(modifiers->findData(binding.enabled ? int(binding.modifiers) : -1));
        modifiers->setToolTip(index == 0u
            ? "Turn the wheel to zoom outside Fly. Active editing gestures and Multi-cut's Ctrl-wheel take priority. No modifiers preserves ordinary wheel zoom; Disabled turns scrolling off. Zoom In and Zoom Out keyboard commands remain available in Keymap > Viewport > Camera."
            : "Turn the wheel to change speed only while Fly Look owns the pointer. Fly consumes its wheel input, including when scrolling is disabled. Use Fly Speed Up and Fly Speed Down in Keymap > Viewport > Camera to adjust speed by keys while Fly is active, including wheel-click Fly.");
        auto* const inverted = new QCheckBox("Invert", wheelSection);
        inverted->setObjectName(QStringLiteral("pref_mouse_%1_inverted")
            .arg(QString::fromLatin1(wheelIds[index])));
        inverted->setProperty("sed.mouseNavigationPreference", true);
        inverted->setChecked(binding.inverted);
        inverted->setEnabled(binding.enabled);
        QObject::connect(modifiers, &QComboBox::currentIndexChanged, inverted,
            [modifiers, inverted](int) { inverted->setEnabled(modifiers->currentData().toInt() >= 0); });
        wheelForm->addWidget(new QLabel(index == 0u ? "Zoom" : "Fly speed", wheelSection), row, 0);
        wheelForm->addWidget(modifiers, row, 1, Qt::AlignLeft);
        wheelForm->addWidget(inverted, row, 2);
        wheelModifiers[index] = modifiers;
        wheelInversion[index] = inverted;
    }
    auto* const wheelKeyboardHint = new QLabel(
        "Keymap: <a href=\"#camera-zoom\">Zoom In/Out</a> · "
        "<a href=\"#camera-fly-speed\">Fly Speed Up/Down</a>", wheelSection);
    wheelKeyboardHint->setObjectName("pref_mouse_wheel_keyboard_hint");
    wheelKeyboardHint->setTextFormat(Qt::RichText);
    wheelKeyboardHint->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    wheelKeyboardHint->setFocusPolicy(Qt::StrongFocus);
    wheelKeyboardHint->setOpenExternalLinks(false);
    wheelKeyboardHint->setAccessibleName("Open Zoom and Fly speed Camera key bindings");
    wheelKeyboardHint->setWordWrap(true);
    wheelForm->addWidget(wheelKeyboardHint, 3, 0, 1, 4);
    const int wheelRow = static_cast<int>(mouseButtons.size() + 1);
    mouseForm->addWidget(wheelSection, wheelRow, 0, 1, 3);
    const int inversionRow = wheelRow + 1;
    auto* const horizontalInversion = new QCheckBox("Invert horizontal", mouseNavigationSection);
    horizontalInversion->setObjectName("pref_mouse_invert_horizontal");
    horizontalInversion->setProperty("sed.mouseNavigationPreference", true);
    horizontalInversion->setChecked(originalMouseNavigation.invertHorizontal);
    auto* const verticalInversion = new QCheckBox("Invert vertical", mouseNavigationSection);
    verticalInversion->setObjectName("pref_mouse_invert_vertical");
    verticalInversion->setProperty("sed.mouseNavigationPreference", true);
    verticalInversion->setChecked(originalMouseNavigation.invertVertical);
    const QString inversionHint = QStringLiteral("Invert this axis of camera mouse motion for Orbit, Pan, Dolly, and Fly Look. Wheel inversion is configured separately.");
    horizontalInversion->setToolTip(inversionHint);
    verticalInversion->setToolTip(inversionHint);
    mouseForm->addWidget(new QLabel("Camera motion", mouseNavigationSection), inversionRow, 0);
    mouseForm->addWidget(horizontalInversion, inversionRow, 1);
    mouseForm->addWidget(verticalInversion, inversionRow, 2);
    auto* const mouseNavigationError = new QLabel(mouseNavigationSection);
    mouseNavigationError->setObjectName("pref_mouse_binding_error");
    mouseNavigationError->setWordWrap(true);
    mouseNavigationError->setStyleSheet("color: #edb76b;");
    mouseForm->addWidget(mouseNavigationError, inversionRow + 1, 0, 1, 4);
    auto* const mouseReset = new QPushButton("Restore Defaults", mouseNavigationSection);
    mouseReset->setObjectName("pref_mouse_reset_defaults");
    mouseForm->addWidget(mouseReset, inversionRow + 2, 0, 1, 4);
    auto* const mouseHint = new QLabel(
        "Pick and edit uses the existing selection and tool modifiers. Its activation modifiers are consumed before editing. "
        "Orbit, Pan, and Dolly can change with their modifiers during the same button drag. "
        "Turning the wheel is separate from pressing it. To use its button for Fly look, choose Middle button (wheel click) and move conflicting Orbit, Pan, or Dolly gestures to another button or modifier. "
        "Disabled turns an action off; No modifiers uses its physical mouse button without an activation key. "
        "These controls apply to the Level viewport; resource previews keep their own controls.", mouseNavigationSection);
    mouseHint->setWordWrap(true);
    mouseForm->addWidget(mouseHint, inversionRow + 3, 0, 1, 4);
    mouseNavigationSection->contentLayout()->addLayout(mouseForm);
    viewportLayout->addWidget(mouseNavigationSection);
    const auto captureMouseNavigationControls = [&]()
    {
        ViewportNavigationSettings result{};
        for ( std::size_t index = 0; index < result.mouse.size(); ++index )
        {
            result.mouse[index] = {
                static_cast<Qt::MouseButton>(mouseButtons[index]->currentData().toInt()),
                Qt::KeyboardModifiers(mouseModifiers[index]->currentData().toInt()),
            };
        }
        for ( std::size_t index = 0; index < wheelModifiers.size(); ++index )
        {
            ViewportWheelBinding& binding = index == 0u ? result.zoom : result.flySpeed;
            const int modifiers = wheelModifiers[index]->currentData().toInt();
            binding.enabled = modifiers >= 0;
            binding.modifiers = modifiers >= 0 ? Qt::KeyboardModifiers(modifiers) : Qt::NoModifier;
            binding.inverted = wheelInversion[index]->isChecked();
        }
        result.invertHorizontal = horizontalInversion->isChecked();
        result.invertVertical = verticalInversion->isChecked();
        return result;
    };
    const auto validateMouseNavigationControls = [&]()
    {
        QString error{};
        mouseNavigationValid = validateViewportNavigationSettings(captureMouseNavigationControls(), &error);
        mouseNavigationError->setText(error);
        mouseNavigationError->setVisible(!error.isEmpty());
        updatePreferenceAcceptState();
        return mouseNavigationValid;
    };
    for ( std::size_t index = 0; index < mouseButtons.size(); ++index )
    {
        QObject::connect(mouseButtons[index], &QComboBox::currentIndexChanged, &dialog,
            [&, index, validateMouseNavigationControls](int)
        {
            mouseModifiers[index]->setEnabled(mouseButtons[index]->currentData().toInt() != int(Qt::NoButton));
            (void)validateMouseNavigationControls();
        });
        QObject::connect(mouseModifiers[index], &QComboBox::currentIndexChanged, &dialog,
            [validateMouseNavigationControls](int) { (void)validateMouseNavigationControls(); });
    }
    QObject::connect(mouseReset, &QPushButton::clicked, &dialog, [&]()
    {
        const ViewportNavigationSettings defaults{};
        for ( std::size_t index = 0; index < defaults.mouse.size(); ++index )
        {
            mouseButtons[index]->setCurrentIndex(mouseButtons[index]->findData(int(defaults.mouse[index].button)));
            mouseModifiers[index]->setCurrentIndex(mouseModifiers[index]->findData(int(defaults.mouse[index].modifiers)));
        }
        for ( std::size_t index = 0; index < wheelModifiers.size(); ++index )
        {
            wheelModifiers[index]->setCurrentIndex(wheelModifiers[index]->findData(0));
            wheelInversion[index]->setChecked(false);
        }
        horizontalInversion->setChecked(false);
        verticalInversion->setChecked(false);
        (void)validateMouseNavigationControls();
    });

    // Navigation section.
    auto* navigationSection = createCollapsedSection(
        QStringLiteral("navigation"), "Navigation", viewportPage);
    {
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        QSettings settings = settingsStore();

        auto* orbitAroundSelection = new QCheckBox("Orbit around selection", navigationSection);
        orbitAroundSelection->setObjectName("pref_viewport_orbit_around_selection");
        orbitAroundSelection->setChecked(settings.value("viewport/orbit_around_selection", true).toBool());
        orbitAroundSelection->setToolTip(
            "Use the current selection as the pivot with the configured Orbit drag gesture. Turn this off to revolve around the current camera target; empty selections already use that behavior.");
        form->addRow("Orbit drag", orbitAroundSelection);

        auto* outlinerDblClickFrame = new QCheckBox("Frame item on double-click", navigationSection);
        outlinerDblClickFrame->setObjectName("pref_outliner_double_click_frame");
        outlinerDblClickFrame->setChecked(settings.value("viewport/outliner_double_click_frame", true).toBool());
        form->addRow("Outliner", outlinerDblClickFrame);

        auto* outlinerDblClickAnimate = new QCheckBox("Animate camera movement", navigationSection);
        outlinerDblClickAnimate->setObjectName("pref_outliner_double_click_frame_animate");
        outlinerDblClickAnimate->setChecked(settings.value("viewport/outliner_double_click_frame_animate", true).toBool());
        outlinerDblClickAnimate->setEnabled(outlinerDblClickFrame->isChecked());
        form->addRow("Animation", outlinerDblClickAnimate);

        auto* outlinerDblClickDistance = new QDoubleSpinBox(navigationSection);
        outlinerDblClickDistance->setObjectName("pref_outliner_double_click_distance_scale");
        outlinerDblClickDistance->setRange(0.10, 10.0);
        outlinerDblClickDistance->setDecimals(2);
        outlinerDblClickDistance->setSingleStep(0.05);
        outlinerDblClickDistance->setValue(settings.value("viewport/outliner_double_click_distance_scale", 1.0).toDouble());
        outlinerDblClickDistance->setSuffix("x");
        outlinerDblClickDistance->setToolTip("Framing distance scale used when double-clicking entities in outliner/item editor.");
        outlinerDblClickDistance->setEnabled(outlinerDblClickFrame->isChecked());
        form->addRow("Frame distance", outlinerDblClickDistance);

        QObject::connect(outlinerDblClickFrame, &QCheckBox::toggled, outlinerDblClickAnimate, &QWidget::setEnabled);
        QObject::connect(outlinerDblClickFrame, &QCheckBox::toggled, outlinerDblClickDistance, &QWidget::setEnabled);

        navigationSection->contentLayout()->addLayout(form);
    }
    viewportLayout->addWidget(navigationSection);

    // Overlays section.
    auto* overlaysSection = createCollapsedSection(
        QStringLiteral("overlays"), "Overlays", viewportPage);
    {
        auto* form = new QFormLayout();
        form->setContentsMargins(6, 4, 6, 4);
        form->setSpacing(6);

        QSettings settings = settingsStore();

        auto* selectedSurfaceNormalsCheck = new QCheckBox(
            "Show selected surface normals", overlaysSection);
        selectedSurfaceNormalsCheck->setObjectName(
            "pref_viewport_show_selected_surface_normals");
        selectedSurfaceNormalsCheck->setToolTip(
            "Draw a cyan arrow from each selected face center along its surface normal."
            " Edge and vertex modes keep the marker on the owning face.");
        selectedSurfaceNormalsCheck->setChecked(
            settings.value("viewport/show_selected_surface_normals", false).toBool());
        form->addRow("Surface normals", selectedSurfaceNormalsCheck);

        auto* surfaceNormalLengthSpin = new QDoubleSpinBox(overlaysSection);
        surfaceNormalLengthSpin->setObjectName(
            "pref_viewport_surface_normal_length");
        surfaceNormalLengthSpin->setRange(
            static_cast<double>(render::kSurfaceNormalLengthScaleMin),
            static_cast<double>(render::kSurfaceNormalLengthScaleMax));
        surfaceNormalLengthSpin->setDecimals(2);
        surfaceNormalLengthSpin->setSingleStep(0.10);
        surfaceNormalLengthSpin->setSuffix("x");
        surfaceNormalLengthSpin->setValue(std::clamp(
            settings.value("viewport/surface_normal_length_scale", 1.0).toDouble(),
            static_cast<double>(render::kSurfaceNormalLengthScaleMin),
            static_cast<double>(render::kSurfaceNormalLengthScaleMax)));
        surfaceNormalLengthSpin->setToolTip(
            "Scale the length of selected-surface normal arrows relative to the face size.");
        form->addRow("Normal length", surfaceNormalLengthSpin);

        auto* surfaceNormalArrowheadSpin = new QDoubleSpinBox(overlaysSection);
        surfaceNormalArrowheadSpin->setObjectName(
            "pref_viewport_surface_normal_arrowhead_size");
        surfaceNormalArrowheadSpin->setRange(
            static_cast<double>(render::kSurfaceNormalArrowheadScaleMin),
            static_cast<double>(render::kSurfaceNormalArrowheadScaleMax));
        surfaceNormalArrowheadSpin->setDecimals(2);
        surfaceNormalArrowheadSpin->setSingleStep(0.10);
        surfaceNormalArrowheadSpin->setSuffix("x");
        surfaceNormalArrowheadSpin->setValue(std::clamp(
            settings.value("viewport/surface_normal_arrowhead_scale", 1.0).toDouble(),
            static_cast<double>(render::kSurfaceNormalArrowheadScaleMin),
            static_cast<double>(render::kSurfaceNormalArrowheadScaleMax)));
        surfaceNormalArrowheadSpin->setToolTip(
            "Scale the two wings at the end of each normal arrow.");
        form->addRow("Normal arrowhead", surfaceNormalArrowheadSpin);

        auto* surfaceNormalArrowheadCheck = new QCheckBox(
            "Show arrowheads", overlaysSection);
        surfaceNormalArrowheadCheck->setObjectName(
            "pref_viewport_surface_normal_show_arrowhead");
        surfaceNormalArrowheadCheck->setChecked(settings.value(
            "viewport/surface_normal_show_arrowhead", true).toBool());
        surfaceNormalArrowheadCheck->setToolTip(
            "Show or hide the camera-facing arrowhead on each normal marker.");
        form->addRow("Normal marker", surfaceNormalArrowheadCheck);

        // Normal overlay preferences are applied immediately while this page
        // is open.  This keeps a long-lived Preferences dialog honest when a
        // marker is being tuned against the viewport; the accepted values are
        // persisted below with the other viewport preferences.
        QObject::connect(surfaceNormalLengthSpin,
            qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](const double value)
            {
                if (glView_)
                {
                    glView_->setSurfaceNormalLengthScale(
                        static_cast<float>(value));
                }
            });
        QObject::connect(surfaceNormalArrowheadSpin,
            qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](const double value)
            {
                if (glView_)
                {
                    glView_->setSurfaceNormalArrowheadScale(
                        static_cast<float>(value));
                }
            });
        QObject::connect(surfaceNormalArrowheadCheck, &QCheckBox::toggled,
            this, [this](const bool enabled)
            {
                if (glView_)
                {
                    glView_->setShowSurfaceNormalArrowhead(enabled);
                }
            });

        auto* previewRespectHidden = new QCheckBox("Hide layers hidden in editor", overlaysSection);
        previewRespectHidden->setObjectName("pref_preview_respect_hidden_layers");
        previewRespectHidden->setChecked(settings.value("viewport/preview_respect_hidden_layers", false).toBool());
        form->addRow("Hidden layers", previewRespectHidden);

        auto* extrudePreviewOpacity = new QSpinBox(overlaysSection);
        extrudePreviewOpacity->setObjectName(
            QStringLiteral("pref_surface_extrude_preview_opacity"));
        extrudePreviewOpacity->setRange(5, 100);
        extrudePreviewOpacity->setSingleStep(5);
        extrudePreviewOpacity->setSuffix(QStringLiteral("%"));
        extrudePreviewOpacity->setValue(qRound(std::clamp(
            settings.value(
                QStringLiteral("viewport/surface_extrude_preview_opacity"),
                0.74).toDouble(),
            0.05,
            1.0) * 100.0));
        extrudePreviewOpacity->setToolTip(
            QStringLiteral("Set the opacity of temporary geometry shown while extruding a surface."));
        form->addRow("Extrude preview", extrudePreviewOpacity);

        overlaysSection->contentLayout()->addLayout(form);
    }
    viewportLayout->addWidget(overlaysSection);

    viewportLayout->addStretch(1);

    auto* viewportScroll = new QScrollArea(&dialog);
    viewportScroll->setWidgetResizable(true);
    viewportScroll->setFrameShape(QFrame::NoFrame);
    viewportScroll->setWidget(viewportPage);

    categories->addItem("Viewport");
    stack->addWidget(viewportScroll);

    // ---- COG editor preferences page ----
    auto* cogEditorPage = new QWidget(&dialog);
    cogEditorPage->setProperty(
        "sed.preferencesDisclosurePath", QStringLiteral("cog-editor"));
    auto* cogEditorLayout = new QVBoxLayout(cogEditorPage);
    cogEditorLayout->setContentsMargins(0, 0, 0, 0);
    cogEditorLayout->setSpacing(12);

    {
        QSettings settings = settingsStore();

        auto* appearanceSection = createCollapsedSection(
            QStringLiteral("appearance"), "Appearance", cogEditorPage);
        {
            auto* form = new QFormLayout();
            form->setContentsMargins(6, 4, 6, 4);
            form->setSpacing(6);

            auto* fontFamilyCombo = new QComboBox(appearanceSection);
            fontFamilyCombo->setObjectName(
                QStringLiteral("pref_cog_editor_font_family"));
            fontFamilyCombo->setEditable(true);
            fontFamilyCombo->setInsertPolicy(QComboBox::NoInsert);
            fontFamilyCombo->setDuplicatesEnabled(false);
            fontFamilyCombo->setMaxVisibleItems(18);
            fontFamilyCombo->addItems(
                ui::cog::availableCogScriptEditorFontFamilies());
            fontFamilyCombo->setEditText(settings.value(
                "cog_editor/font_family",
                ui::cog::defaultCogScriptEditorFontFamilyList()).toString());
            fontFamilyCombo->setToolTip(QStringLiteral(
                "Choose an installed code font, or type an ordered comma-separated fallback list."));
            if (QCompleter* completer = fontFamilyCombo->completer())
            {
                completer->setCaseSensitivity(Qt::CaseInsensitive);
                completer->setCompletionMode(QCompleter::PopupCompletion);
                completer->setFilterMode(Qt::MatchContains);
            }
            if (QAbstractItemView* popup = fontFamilyCombo->view())
            {
                // The closed combo deliberately ignores wheel input so a
                // passing pointer cannot change the chosen font.  Its open
                // installed-font list is a scroll view, however, and must
                // retain ordinary wheel/touchpad navigation.
                popup->setObjectName(
                    QStringLiteral("pref_cog_editor_font_family_popup"));
                popup->setProperty("sed.comboPopupWheel", true);
                if (popup->viewport())
                {
                    popup->viewport()->setProperty(
                        "sed.comboPopupWheel", true);
                }
                popup->setVerticalScrollMode(
                    QAbstractItemView::ScrollPerPixel);
            }
            form->addRow("Font family", fontFamilyCombo);

            auto* fontSizeSpin = new QSpinBox(appearanceSection);
            fontSizeSpin->setObjectName(QStringLiteral("pref_cog_editor_font_size"));
            fontSizeSpin->setRange(7, 32);
            fontSizeSpin->setSuffix(QStringLiteral(" pt"));
            fontSizeSpin->setValue(std::clamp(
                settings.value("cog_editor/font_size", 10).toInt(), 7, 32));
            fontSizeSpin->setToolTip(QStringLiteral(
                "Base COG source font size at 100% editor zoom."));
            form->addRow("Font size", fontSizeSpin);

            auto* fontStyleRow = new QWidget(appearanceSection);
            auto* fontStyleLayout = new QHBoxLayout(fontStyleRow);
            fontStyleLayout->setContentsMargins(0, 0, 0, 0);
            fontStyleLayout->setSpacing(10);
            auto* fontBoldCheck = new QCheckBox("Bold", fontStyleRow);
            fontBoldCheck->setObjectName(QStringLiteral("pref_cog_editor_font_bold"));
            fontBoldCheck->setChecked(
                settings.value("cog_editor/font_bold", false).toBool());
            auto* fontItalicCheck = new QCheckBox("Italic", fontStyleRow);
            fontItalicCheck->setObjectName(QStringLiteral("pref_cog_editor_font_italic"));
            fontItalicCheck->setChecked(
                settings.value("cog_editor/font_italic", false).toBool());
            fontStyleLayout->addWidget(fontBoldCheck);
            fontStyleLayout->addWidget(fontItalicCheck);
            fontStyleLayout->addStretch(1);
            form->addRow("Font style", fontStyleRow);

            const auto previewFont =
                [this, fontFamilyCombo, fontSizeSpin, fontBoldCheck,
                 fontItalicCheck, &cogFontPreviewChanged]()
            {
                cogFontPreviewChanged = true;
                previewCogScriptEditorFont(ui::cog::makeCogScriptEditorFont(
                    fontFamilyCombo->currentText(),
                    fontSizeSpin->value(),
                    fontBoldCheck->isChecked(),
                    fontItalicCheck->isChecked()));
            };
            if (fontFamilyCombo->lineEdit())
            {
                QObject::connect(fontFamilyCombo->lineEdit(),
                    &QLineEdit::editingFinished, &dialog, previewFont);
            }
            QObject::connect(fontFamilyCombo,
                qOverload<int>(&QComboBox::activated), &dialog,
                [previewFont](int) { previewFont(); });
            QObject::connect(fontSizeSpin, qOverload<int>(&QSpinBox::valueChanged),
                &dialog, [previewFont](int) { previewFont(); });
            QObject::connect(fontBoldCheck, &QCheckBox::toggled,
                &dialog, [previewFont](bool) { previewFont(); });
            QObject::connect(fontItalicCheck, &QCheckBox::toggled,
                &dialog, [previewFont](bool) { previewFont(); });

            auto* themeCombo = new QComboBox(appearanceSection);
            themeCombo->setObjectName("pref_cog_editor_theme");
            const QString currentThemeId = settings.value(
                    "cog_editor/theme", defaultCogScriptTheme().id).toString();
            form->addRow("Theme", themeCombo);

            auto serializeTheme = [](const CogScriptTheme& theme) -> QVariantMap
            {
                QVariantMap map{};
                map.insert(QStringLiteral("id"), theme.id);
                map.insert(QStringLiteral("label"), theme.label);
                QVariantMap colors{};
                for ( const CogScriptThemeColorDef& def : cogScriptThemeColorDefs() )
                {
                    colors.insert(QString::fromLatin1(def.key),
                        (theme.*(def.member)).name(QColor::HexArgb));
                }
                map.insert(QStringLiteral("colors"), colors);
                return map;
            };

            auto deserializeTheme = [](const QVariantMap& map) -> CogScriptTheme
            {
                CogScriptTheme theme = defaultCogScriptTheme();
                theme.id = map.value(QStringLiteral("id")).toString().trimmed();
                theme.label = map.value(QStringLiteral("label")).toString().trimmed();
                const QVariantMap colors = map.value(QStringLiteral("colors")).toMap();
                for ( const CogScriptThemeColorDef& def : cogScriptThemeColorDefs() )
                {
                    const QColor color(colors.value(QString::fromLatin1(def.key)).toString().trimmed());
                    if ( color.isValid() )
                    {
                        theme.*(def.member) = color;
                    }
                }
                return theme;
            };

            QVariantList customThemeData{};
            QString cogThemeLoadError{};
            const std::vector<CogScriptTheme> loadedCogThemes =
                customCogScriptThemes(settings, &cogThemeLoadError);
            for ( const CogScriptTheme& theme : loadedCogThemes )
            {
                customThemeData.push_back(serializeTheme(theme));
            }
            if ( !cogThemeLoadError.isEmpty() )
            {
                QMessageBox::warning(
                    &dialog,
                    QStringLiteral("COG Themes Could Not Be Fully Loaded"),
                    cogThemeLoadError);
            }
            themeCombo->setProperty("sed.customThemeData", customThemeData);

            auto findThemeById = [themeCombo, deserializeTheme](const QString& themeId) -> CogScriptTheme
            {
                const QString trimmedId = themeId.trimmed();
                for ( const QVariant& value : themeCombo->property("sed.customThemeData").toList() )
                {
                    const CogScriptTheme customTheme = deserializeTheme(value.toMap());
                    if ( customTheme.id.compare(trimmedId, Qt::CaseInsensitive) == 0 )
                    {
                        return customTheme;
                    }
                }
                return cogScriptThemeById(trimmedId);
            };

            auto rebuildThemeCombo = [themeCombo, deserializeTheme](const QString& selectedId)
            {
                const QSignalBlocker blocker(themeCombo);
                themeCombo->clear();
                int currentThemeIndex = 0;
                int index = 0;
                for ( const CogScriptTheme& theme : cogScriptThemes() )
                {
                    themeCombo->addItem(theme.label, theme.id);
                    if ( theme.id.compare(selectedId, Qt::CaseInsensitive) == 0 )
                    {
                        currentThemeIndex = index;
                    }
                    ++index;
                }
                const QVariantList customThemes = themeCombo->property("sed.customThemeData").toList();
                for ( const QVariant& value : customThemes )
                {
                    const CogScriptTheme theme = deserializeTheme(value.toMap());
                    if ( theme.id.isEmpty() || theme.label.isEmpty() )
                    {
                        continue;
                    }
                    themeCombo->addItem(theme.label, theme.id);
                    if ( theme.id.compare(selectedId, Qt::CaseInsensitive) == 0 )
                    {
                        currentThemeIndex = index;
                    }
                    ++index;
                }
                themeCombo->setCurrentIndex(currentThemeIndex);
            };

            rebuildThemeCombo(currentThemeId);
            const auto cogThemesFromUi = [themeCombo, deserializeTheme]()
            {
                std::vector<CogScriptTheme> themes{};
                const QVariantList values =
                    themeCombo->property("sed.customThemeData").toList();
                themes.reserve(static_cast<std::size_t>(values.size()));
                for ( const QVariant& value : values )
                {
                    const CogScriptTheme theme = deserializeTheme(value.toMap());
                    if ( !theme.id.isEmpty() && !theme.label.isEmpty() )
                    {
                        themes.push_back(theme);
                    }
                }
                return themes;
            };
            persistCogThemeFiles = [this, cogThemesFromUi](QString* error)
            {
                QSettings themeSettings = settingsStore();
                return saveCustomCogScriptThemes(
                    themeSettings, cogThemesFromUi(), error);
            };
            const auto persistCogThemeMutation = [&]()
            {
                QString error{};
                if (persistCogThemeFiles(&error))
                {
                    cogThemeFilesDirty = false;
                    return true;
                }
                QMessageBox::warning(
                    &dialog,
                    QStringLiteral("Could Not Save COG Theme"),
                    error.isEmpty() ? QStringLiteral("The COG theme could not be saved.") : error);
                return false;
            };
            preflightCogThemeFiles = [cogThemesFromUi](
                QSettings& themeSettings, QString* error)
            {
                return preflightCustomCogScriptThemes(
                    themeSettings, cogThemesFromUi(), error);
            };

            auto* themeButtons = new QWidget(appearanceSection);
            auto* themeButtonsLayout = new QHBoxLayout(themeButtons);
            themeButtonsLayout->setContentsMargins(0, 0, 0, 0);
            themeButtonsLayout->setSpacing(6);
            auto* saveThemeButton = new QPushButton("Add New...", themeButtons);
            saveThemeButton->setObjectName("pref_cog_editor_theme_add_new");
            auto* updateThemeButton = new QPushButton("Update Theme", themeButtons);
            updateThemeButton->setObjectName("pref_cog_editor_theme_update");
            auto* deleteThemeButton = new QPushButton("Delete Theme", themeButtons);
            deleteThemeButton->setObjectName("pref_cog_editor_theme_delete");
            saveThemeButton->setToolTip("Copy the selected theme and current color overrides into a new user theme.");
            updateThemeButton->setToolTip("Save the current color edits to the selected user theme.");
            deleteThemeButton->setToolTip("Delete the selected user theme.");
            themeButtonsLayout->addWidget(saveThemeButton);
            themeButtonsLayout->addWidget(updateThemeButton);
            themeButtonsLayout->addWidget(deleteThemeButton);
            themeButtonsLayout->addStretch(1);
            form->addRow(QString(), themeButtons);

            auto* whitespaceCheck = new QCheckBox("Show whitespace by default", appearanceSection);
            whitespaceCheck->setObjectName("pref_cog_editor_show_whitespace");
            whitespaceCheck->setChecked(settings.value("cog_editor/show_whitespace", false).toBool());
            form->addRow("Whitespace", whitespaceCheck);

            appearanceSection->contentLayout()->addLayout(form);

            auto* colorsSection = createCollapsedSection(
                // Keep the disclosure id stable for the session while the
                // section now also contains Find/reference overlay colors.
                QStringLiteral("syntax-colors"), "Editor Colors",
                appearanceSection);
            auto* colorsForm = new QFormLayout();
            colorsForm->setContentsMargins(6, 4, 6, 4);
            colorsForm->setSpacing(6);

            auto updateColorSwatch = [](QToolButton* button, const QColor& color)
            {
                if ( !button )
                {
                    return;
                }

                button->setProperty("sed.colorValue", color);
                button->setToolTip(color.name(QColor::HexArgb));
                button->setStyleSheet(QString(
                    "QToolButton {"
                    "  background: %1;"
                    "  border: 1px solid rgba(0, 0, 0, 160);"
                    "  border-radius: 6px;"
                    "  min-width: 52px;"
                    "  min-height: 18px;"
                    "}"
                    "QToolButton:hover {"
                    "  border-color: rgba(255, 255, 255, 120);"
                    "}").arg(color.name(QColor::HexArgb)));
            };

            auto applyThemeColorsToButtons =
                [updateColorSwatch](const CogScriptTheme& theme, const QHash<QString, QToolButton*>& buttons)
            {
                for ( const CogScriptThemeColorDef& def : cogScriptThemeColorDefs() )
                {
                    if ( auto it = buttons.find(QString::fromLatin1(def.key)); it != buttons.end() )
                    {
                        updateColorSwatch(it.value(), theme.*(def.member));
                    }
                }
            };

            QHash<QString, QToolButton*> colorButtons{};
            colorButtons.reserve(static_cast<int>(cogScriptThemeColorDefs().size()));

            const CogScriptTheme resolvedTheme = resolveCogScriptTheme(settings);
            for ( const CogScriptThemeColorDef& def : cogScriptThemeColorDefs() )
            {
                auto* swatch = new QToolButton(colorsSection);
                swatch->setObjectName(QStringLiteral("pref_cog_editor_color_%1").arg(def.key));
                swatch->setAutoRaise(true);
                swatch->setCursor(Qt::PointingHandCursor);
                swatch->setText(QString());
                updateColorSwatch(swatch, resolvedTheme.*(def.member));
                QObject::connect(
                    swatch,
                    &QToolButton::clicked,
                    &dialog,
                    [swatch, label = QString::fromLatin1(def.label), updateColorSwatch, &dialog]()
                    {
                        const QColor current = swatch->property("sed.colorValue").value<QColor>();
                        const QColor chosen = QColorDialog::getColor(current, &dialog, QStringLiteral("Select %1 color").arg(label));
                        if ( !chosen.isValid() )
                        {
                            return;
                        }
                        updateColorSwatch(swatch, chosen);
                    });
                colorsForm->addRow(QString::fromLatin1(def.label), swatch);
                colorButtons.insert(QString::fromLatin1(def.key), swatch);
            }

            auto* resetColorsButton = new QPushButton("Reset to Theme", colorsSection);
            QObject::connect(
                resetColorsButton,
                &QPushButton::clicked,
                &dialog,
                [themeCombo, colorButtons, findThemeById, applyThemeColorsToButtons]()
                {
                    const CogScriptTheme theme = findThemeById(themeCombo->currentData().toString());
                    applyThemeColorsToButtons(theme, colorButtons);
                });
            colorsForm->addRow(QString(), resetColorsButton);

            QObject::connect(
                themeCombo,
                &QComboBox::currentIndexChanged,
                &dialog,
                [themeCombo, updateThemeButton, deleteThemeButton, colorButtons, findThemeById, applyThemeColorsToButtons](int)
                {
                    if ( !themeCombo || themeCombo->count() <= 0 || themeCombo->currentIndex() < 0 )
                    {
                        updateThemeButton->setEnabled(false);
                        deleteThemeButton->setEnabled(false);
                        return;
                    }

                    const QString currentId = themeCombo->currentData().toString();
                    if ( currentId.isEmpty() )
                    {
                        updateThemeButton->setEnabled(false);
                        deleteThemeButton->setEnabled(false);
                        return;
                    }

                    const bool isCustom = currentId.startsWith(QStringLiteral("custom_"), Qt::CaseInsensitive);
                    updateThemeButton->setEnabled(isCustom);
                    deleteThemeButton->setEnabled(isCustom);
                    const CogScriptTheme theme = findThemeById(currentId);
                    applyThemeColorsToButtons(theme, colorButtons);
                });

            QObject::connect(
                saveThemeButton,
                &QPushButton::clicked,
                &dialog,
                [themeCombo,
                 updateThemeButton,
                 deleteThemeButton,
                 colorButtons,
                 serializeTheme,
                 findThemeById,
                 rebuildThemeCombo,
                 applyThemeColorsToButtons,
                 persistCogThemeMutation,
                 &cogThemeFilesDirty,
                 &dialog]()
                {
                    ThemeNameInputDialog nameDialog(&dialog);
                    nameDialog.setObjectName(
                        QStringLiteral("pref_cog_theme_name_dialog"));
                    nameDialog.setWindowTitle(QStringLiteral("Add COG Theme"));
                    nameDialog.setLabelText(
                        QStringLiteral("Name for the copied theme"));
                    nameDialog.setWindowFlag(Qt::WindowMinimizeButtonHint, false);
                    nameDialog.setWindowFlag(Qt::WindowMaximizeButtonHint, false);
                    nameDialog.setWindowFlag(Qt::WindowMinMaxButtonsHint, false);
                    nameDialog.setMinimumSize(620, 180);
                    nameDialog.resize(620, 180);
                    if (nameDialog.exec() != QDialog::Accepted)
                    {
                        return;
                    }
                    QString label = nameDialog.textValue().trimmed();
                    if (label.isEmpty())
                    {
                        return;
                    }

                    if ( label.size() > 128 )
                    {
                        QMessageBox::warning(
                            &dialog,
                            QStringLiteral("Theme Name Is Too Long"),
                            QStringLiteral("Theme names must be 128 characters or fewer."));
                        return;
                    }
                    const auto sameName = [&](const QString& existing)
                    { return ui::theme_storage::themeNamesCollide(existing, label); };
                    const bool collidesWithBuiltIn = std::any_of(
                        cogScriptThemes().begin(), cogScriptThemes().end(),
                        [&](const CogScriptTheme& item) { return sameName(item.label); });
                    QVariantList themes = themeCombo->property("sed.customThemeData").toList();
                    const bool collidesWithUser = std::any_of(
                        themes.begin(), themes.end(), [&](const QVariant& value)
                        { return sameName(value.toMap().value(QStringLiteral("label")).toString()); });
                    if (collidesWithBuiltIn || collidesWithUser)
                    {
                        QMessageBox::warning(
                            &dialog,
                            QStringLiteral("Theme Name Already Exists"),
                            QStringLiteral("A COG theme with this name already exists."));
                        return;
                    }
                    const QString uniqueId = QStringLiteral("custom_%1").arg(
                        ui::theme_storage::themeSlugFromLabel(label));

                    const QString previousId = themeCombo->currentData().toString();
                    const CogScriptTheme previousTheme = findThemeById(previousId);
                    const QVariantList previousThemes = themes;
                    CogScriptTheme theme = previousTheme;
                    theme.id = uniqueId;
                    theme.label = label;
                    for ( const CogScriptThemeColorDef& def : cogScriptThemeColorDefs() )
                    {
                        if ( auto it = colorButtons.find(QString::fromLatin1(def.key));
                            it != colorButtons.end() )
                        {
                            const QColor color = it.value()->property("sed.colorValue").value<QColor>();
                            if ( color.isValid() )
                            {
                                theme.*(def.member) = color;
                            }
                        }
                    }

                    themes.push_back(serializeTheme(theme));
                    cogThemeFilesDirty = true;
                    themeCombo->setProperty("sed.customThemeData", themes);
                    if (!persistCogThemeMutation())
                    {
                        themeCombo->setProperty("sed.customThemeData", previousThemes);
                        rebuildThemeCombo(previousId);
                        applyThemeColorsToButtons(previousTheme, colorButtons);
                        return;
                    }
                    rebuildThemeCombo(theme.id);
                    applyThemeColorsToButtons(theme, colorButtons);
                    updateThemeButton->setEnabled(true);
                    deleteThemeButton->setEnabled(true);
                });

            QObject::connect(
                updateThemeButton,
                &QPushButton::clicked,
                &dialog,
                [themeCombo, colorButtons, rebuildThemeCombo, findThemeById,
                    applyThemeColorsToButtons, persistCogThemeMutation,
                    &cogThemeFilesDirty]()
                {
                    const QString currentId = themeCombo->currentData().toString();
                    if ( !currentId.startsWith(QStringLiteral("custom_"), Qt::CaseInsensitive) )
                    {
                        return;
                    }

                    const QVariantList previousThemes =
                        themeCombo->property("sed.customThemeData").toList();
                    QVariantList themes = previousThemes;
                    for ( QVariant& value : themes )
                    {
                        QVariantMap map = value.toMap();
                        if ( map.value(QStringLiteral("id")).toString().compare(
                                currentId, Qt::CaseInsensitive) != 0 )
                        {
                            continue;
                        }

                        QVariantMap colors = map.value(QStringLiteral("colors")).toMap();
                        for ( const CogScriptThemeColorDef& def : cogScriptThemeColorDefs() )
                        {
                            if ( auto it = colorButtons.find(QString::fromLatin1(def.key));
                                it != colorButtons.end() )
                            {
                                const QColor color = it.value()->property("sed.colorValue").value<QColor>();
                                if ( color.isValid() )
                                {
                                    colors.insert(QString::fromLatin1(def.key),
                                        color.name(QColor::HexArgb));
                                }
                            }
                        }
                        map.insert(QStringLiteral("colors"), colors);
                        value = map;
                        break;
                    }
                    themeCombo->setProperty("sed.customThemeData", themes);
                    cogThemeFilesDirty = true;
                    if (!persistCogThemeMutation())
                    {
                        themeCombo->setProperty("sed.customThemeData", previousThemes);
                        rebuildThemeCombo(currentId);
                        applyThemeColorsToButtons(findThemeById(currentId), colorButtons);
                        return;
                    }
                    rebuildThemeCombo(currentId);
                    applyThemeColorsToButtons(findThemeById(currentId), colorButtons);
                });

            QObject::connect(
                deleteThemeButton,
                &QPushButton::clicked,
                &dialog,
                [this, &dialog, themeCombo, colorButtons, rebuildThemeCombo, findThemeById,
                    applyThemeColorsToButtons, persistCogThemeMutation,
                    &cogThemeFilesDirty]()
                {
                    const QString currentId = themeCombo->currentData().toString();
                    if ( !currentId.startsWith(QStringLiteral("custom_"), Qt::CaseInsensitive) )
                    {
                        return;
                    }

                    const QVariantList previousThemes =
                        themeCombo->property("sed.customThemeData").toList();
                    QVariantList themes = previousThemes;
                    for ( int i = 0; i < themes.size(); ++i )
                    {
                        if ( themes[i].toMap().value(QStringLiteral("id")).toString().compare(
                                currentId, Qt::CaseInsensitive) == 0 )
                        {
                            themes.removeAt(i);
                            cogThemeFilesDirty = true;
                            break;
                        }
                    }
                    themeCombo->setProperty("sed.customThemeData", themes);
                    if (!persistCogThemeMutation())
                    {
                        themeCombo->setProperty("sed.customThemeData", previousThemes);
                        rebuildThemeCombo(currentId);
                        applyThemeColorsToButtons(findThemeById(currentId), colorButtons);
                        return;
                    }
                    QSettings settings = settingsStore();
                    QString removeError{};
                    const bool removedEverywhere =
                        ui::theme_storage::removeThemeDocumentByIdFromDirectories(
                            ui::theme_storage::directoriesForSettings(
                                settings, sed::AppThemeKind::cog),
                            currentId, ui::theme_storage::ThemeFormat::cog,
                            &removeError);
                    if (!removedEverywhere)
                    {
                        QMessageBox::warning(
                            &dialog,
                            QStringLiteral("Could Not Fully Delete COG Theme"),
                            removeError.isEmpty()
                                ? QStringLiteral("A COG theme file could not be removed.")
                                : removeError);
                    }
                    settings.setValue(QStringLiteral("cog_editor/theme"),
                        defaultCogScriptTheme().id);
                    settings.sync();
                    rebuildThemeCombo(defaultCogScriptTheme().id);
                    applyThemeColorsToButtons(defaultCogScriptTheme(), colorButtons);
                });

            updateThemeButton->setEnabled(themeCombo->currentData().toString().startsWith(QStringLiteral("custom_"), Qt::CaseInsensitive));
            deleteThemeButton->setEnabled(themeCombo->currentData().toString().startsWith(QStringLiteral("custom_"), Qt::CaseInsensitive));

            colorsSection->contentLayout()->addLayout(colorsForm);
            appearanceSection->contentLayout()->addWidget(colorsSection);
        }
        cogEditorLayout->addWidget(appearanceSection);

        auto* formattingSection = createCollapsedSection(
            QStringLiteral("formatting"), "Formatting", cogEditorPage);
        {
            auto* form = new QFormLayout();
            form->setContentsMargins(6, 4, 6, 4);
            form->setSpacing(6);

            auto* tabsCheck = new QCheckBox("Use tabs", formattingSection);
            tabsCheck->setObjectName("pref_cog_editor_format_tabs");
            tabsCheck->setChecked(settings.value("cog_editor/format_use_tabs", false).toBool());
            form->addRow("Indentation", tabsCheck);

            auto* indentSpin = new QSpinBox(formattingSection);
            indentSpin->setObjectName("pref_cog_editor_format_indent");
            indentSpin->setRange(1, 8);
            indentSpin->setValue(std::clamp(settings.value("cog_editor/format_indent_width", 4).toInt(), 1, 8));
            indentSpin->setEnabled(!tabsCheck->isChecked());
            form->addRow("Spaces", indentSpin);

            auto* braceCombo = new QComboBox(formattingSection);
            braceCombo->setObjectName("pref_cog_editor_format_braces");
            braceCombo->addItem("Same line", QStringLiteral("same_line"));
            braceCombo->addItem("Next line", QStringLiteral("next_line"));
            {
                const QString currentBraceStyle =
                    settings.value("cog_editor/format_brace_style", QStringLiteral("next_line")).toString();
                const int braceIndex = std::max(0, braceCombo->findData(currentBraceStyle));
                braceCombo->setCurrentIndex(braceIndex);
            }
            form->addRow("Braces", braceCombo);

            auto* indentSectionsCheck = new QCheckBox("Indent section bodies", formattingSection);
            indentSectionsCheck->setObjectName("pref_cog_editor_format_indent_sections");
            indentSectionsCheck->setChecked(settings.value("cog_editor/format_indent_sections", true).toBool());
            form->addRow("Sections", indentSectionsCheck);

            auto* indentCodeBodiesCheck = new QCheckBox("Indent handler/label bodies", formattingSection);
            indentCodeBodiesCheck->setObjectName("pref_cog_editor_format_indent_code_bodies");
            indentCodeBodiesCheck->setChecked(settings.value("cog_editor/format_indent_code_bodies", true).toBool());
            form->addRow("Handlers", indentCodeBodiesCheck);

            auto* keywordSpacingCheck = new QCheckBox("Space after control keywords", formattingSection);
            keywordSpacingCheck->setObjectName("pref_cog_editor_format_keyword_spacing");
            keywordSpacingCheck->setToolTip("Format if/while/for/switch as `if (condition)`.");
            keywordSpacingCheck->setChecked(settings.value("cog_editor/format_keyword_spacing", true).toBool());
            form->addRow("Keywords", keywordSpacingCheck);

            auto* insideParenthesesCheck = new QCheckBox(
                "Space inside condition/group parentheses", formattingSection);
            insideParenthesesCheck->setObjectName("pref_cog_editor_format_inside_parentheses_spacing");
            insideParenthesesCheck->setToolTip(
                "Format control conditions and grouped expressions as `( value )`; function calls are configured separately.");
            insideParenthesesCheck->setChecked(settings.value("cog_editor/format_inside_parentheses_spacing", false).toBool());
            form->addRow("Parentheses", insideParenthesesCheck);

            auto* functionParenthesesCheck = new QCheckBox(
                "Space inside function calls", formattingSection);
            functionParenthesesCheck->setObjectName(
                "pref_cog_editor_format_function_parentheses_spacing");
            functionParenthesesCheck->setToolTip(
                "Format `Function(value)` as `Function( value )`.");
            functionParenthesesCheck->setChecked(settings.value(
                "cog_editor/format_function_parentheses_spacing", false).toBool());
            form->addRow("Function calls", functionParenthesesCheck);

            auto* forSemicolonCheck = new QCheckBox("Space after for semicolons", formattingSection);
            forSemicolonCheck->setObjectName("pref_cog_editor_format_for_semicolon_spacing");
            forSemicolonCheck->setToolTip("Format for clauses as `for (a; b; c)`.");
            forSemicolonCheck->setChecked(settings.value("cog_editor/format_for_semicolon_spacing", true).toBool());
            form->addRow("For clauses", forSemicolonCheck);

            auto* finalReturnCheck = new QCheckBox("Align final return with handler label", formattingSection);
            finalReturnCheck->setObjectName("pref_cog_editor_format_align_final_return_label");
            finalReturnCheck->setToolTip("Align a handler's final return statement with its label.");
            finalReturnCheck->setChecked(settings.value("cog_editor/format_align_final_return_label", false).toBool());
            form->addRow("Final return", finalReturnCheck);

            auto* indentCommentsCheck = new QCheckBox("Indent standalone comments", formattingSection);
            indentCommentsCheck->setObjectName("pref_cog_editor_format_indent_comments");
            indentCommentsCheck->setToolTip(
                "Align comment-only lines with their surrounding section or code scope.");
            indentCommentsCheck->setChecked(settings.value("cog_editor/format_indent_comments", false).toBool());
            form->addRow("Comments", indentCommentsCheck);

            auto* formatOnSaveCheck = new QCheckBox("Format automatically before saving", formattingSection);
            formatOnSaveCheck->setObjectName("pref_cog_editor_format_on_save");
            formatOnSaveCheck->setToolTip("Apply these formatting rules before writing a COG file.");
            formatOnSaveCheck->setChecked(settings.value("cog_editor/format_on_save", false).toBool());
            form->addRow("Save", formatOnSaveCheck);

            QObject::connect(tabsCheck, &QCheckBox::toggled, indentSpin, [indentSpin](const bool checked)
                {
                    indentSpin->setEnabled(!checked);
                });

            formattingSection->contentLayout()->addLayout(form);
        }
        cogEditorLayout->addWidget(formattingSection);
    }

    cogEditorLayout->addStretch(1);

    auto* cogEditorScroll = new QScrollArea(&dialog);
    cogEditorScroll->setWidgetResizable(true);
    cogEditorScroll->setFrameShape(QFrame::NoFrame);
    cogEditorScroll->setWidget(cogEditorPage);

    categories->addItem("COG Script Editor");
    stack->addWidget(cogEditorScroll);

    // ---- Keymap preferences page ----
    auto* keymapPage = new QWidget(&dialog);
    auto* keymapLayout = new QVBoxLayout(keymapPage);
    keymapLayout->setContentsMargins(0, 0, 0, 0);
    keymapLayout->setSpacing(8);

    auto* keymapSearch = new QLineEdit(keymapPage);
    keymapSearch->setObjectName("pref_keymap_search");
    keymapSearch->setPlaceholderText("Search by action name or key-binding");
    keymapLayout->addWidget(keymapSearch);

    auto* keymapTree = new PreferenceKeymapTree(keymapPage);
    keymapTree->setObjectName("pref_keymap_tree");
    keymapTree->setColumnCount(2);
    keymapTree->setHeaderLabels({ "Action", "Key-Binding" });
    keymapTree->setRootIsDecorated(true);
    // Embedded QKeySequenceEdit widgets are taller than plain item text on
    // some Windows display/font scales.  Let each leaf advertise its real
    // height instead of clipping the shortcut text to a uniform tree row.
    keymapTree->setUniformRowHeights(false);
    keymapTree->setSelectionMode(QAbstractItemView::NoSelection);
    keymapTree->setItemDelegate(new PreferenceKeymapDelegate(keymapTree));
    keymapTree->setAllColumnsShowFocus(false);
    keymapTree->setAlternatingRowColors(true);
    if (auto* header = keymapTree->header())
    {
        header->setStretchLastSection(true);
        header->setSectionResizeMode(0, QHeaderView::Interactive);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
    }
    keymapTree->setColumnWidth(0, 360);
    keymapLayout->addWidget(keymapTree, 1);

    auto* keymapFooter = new QWidget(keymapPage);
    auto* keymapFooterLayout = new QHBoxLayout(keymapFooter);
    keymapFooterLayout->setContentsMargins(0, 0, 0, 0);
    keymapFooterLayout->setSpacing(6);

    auto* keymapHint = new QLabel("Choose an alternative to edit. Use + and − to add or remove shortcuts.", keymapFooter);
    keymapHint->setStyleSheet("color: rgba(220, 220, 220, 180);");
    keymapFooterLayout->addWidget(keymapHint);
    keymapFooterLayout->addStretch(1);

    auto* keymapResetDefaults = new QPushButton("Reset to Defaults", keymapFooter);
    keymapFooterLayout->addWidget(keymapResetDefaults);
    keymapLayout->addWidget(keymapFooter);
    auto* const keymapError = new QLabel(keymapPage);
    keymapError->setObjectName("pref_keymap_binding_error");
    keymapError->setWordWrap(true);
    keymapError->setStyleSheet("color: #edb76b;");
    keymapError->hide();
    keymapLayout->addWidget(keymapError);

    auto makeAutoKeymapBase = [](const QAction* action) -> std::string
    {
        if (!action)
        {
            return "action";
        }

        QString seed = action->text().trimmed();
        seed.remove('&');
        if (seed.isEmpty())
        {
            seed = action->toolTip().trimmed();
        }
        if (seed.isEmpty())
        {
            seed = action->statusTip().trimmed();
        }
        if (seed.isEmpty())
        {
            seed = action->whatsThis().trimmed();
        }
        if (seed.isEmpty())
        {
            seed = QString::fromLatin1(action->metaObject()->className());
        }
        seed = seed.toLower();

        std::string out{};
        out.reserve(static_cast<std::size_t>(seed.size()));
        bool lastUnderscore = false;
        for (const QChar ch : seed)
        {
            const ushort code = ch.unicode();
            const bool isAlphaNum = (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9');
            if (isAlphaNum)
            {
                out.push_back(static_cast<char>(code));
                lastUnderscore = false;
            }
            else if (!lastUnderscore)
            {
                out.push_back('_');
                lastUnderscore = true;
            }
        }

        while (!out.empty() && out.front() == '_')
        {
            out.erase(out.begin());
        }
        while (!out.empty() && out.back() == '_')
        {
            out.pop_back();
        }

        if (out.empty())
        {
            out = "action";
        }
        return out;
    };

    std::unordered_map<std::string, int> autoKeymapCounts{};
    auto keymapIdForAction = [&autoKeymapCounts, makeAutoKeymapBase](QAction* action) -> QString
    {
        if (!action)
        {
            return {};
        }

        const QString explicitId = action->objectName().trimmed();
        if (!explicitId.isEmpty())
        {
            return explicitId;
        }

        const QString cachedAutoId = action->property("sed_generated_keymap_id").toString().trimmed();
        if (!cachedAutoId.isEmpty())
        {
            return cachedAutoId;
        }

        const std::string base = makeAutoKeymapBase(action);
        int& ordinal = autoKeymapCounts[base];
        ++ordinal;

        const QString generated = QString("auto.%1.%2").arg(QString::fromStdString(base)).arg(ordinal);
        action->setProperty("sed_generated_keymap_id", generated);
        return generated;
    };

    auto actionLabelForKeymap = [](const QAction* action) -> QString
    {
        if (!action)
        {
            return QString();
        }

        const QString layerName = action->property("sed.layerName").toString().trimmed().isEmpty()
            ? action->property("sed_layer_name").toString().trimmed()
            : action->property("sed.layerName").toString().trimmed();
        if (!layerName.isEmpty())
        {
            // Visibility and lock bindings are generated for every layer and
            // therefore intentionally share the same semantic layer name.
            // Keep the friendly `Layer: <name>` prefix requested by users,
            // but distinguish the two commands so Layer0 (and every other
            // layer) is not shown as duplicate rows in Preferences.
            QString role = action->property("sed.layerAction").toString().trimmed();
            const QString objectName = action->objectName().trimmed().toLower();
            if (role.isEmpty() && objectName.endsWith(QStringLiteral(".visibility")))
            {
                role = QStringLiteral("Visibility");
            }
            else if (role.isEmpty() && objectName.endsWith(QStringLiteral(".lock")))
            {
                role = QStringLiteral("Lock");
            }
            return role.isEmpty()
                ? QStringLiteral("Layer: %1").arg(layerName)
                : QStringLiteral("Layer: %1 (%2)").arg(layerName, role);
        }

        QString text = action->text().trimmed();
        text.remove('&');
        if ( action->objectName().startsWith(QStringLiteral("cog_editor.")) )
        {
            for ( const QString& prefix : { QStringLiteral("COG Script Editor:"), QStringLiteral("COG Editor:") } )
            {
                if ( text.startsWith(prefix, Qt::CaseInsensitive) )
                {
                    text = text.mid(prefix.size()).trimmed();
                    break;
                }
            }
        }
        if (text.isEmpty())
        {
            text = action->objectName().trimmed();
        }
        if (text.isEmpty())
        {
            text = action->property("sed_generated_keymap_id").toString().trimmed();
        }
        return text;
    };

    auto isInternalWidgetAction = [](const QAction* action) -> bool
    {
        if ( action && !action->objectName().isEmpty()
            && action->property("sed_keymap_expose").toBool() )
        {
            // Persistent editor commands may be hosted inside a scroll panel.
            // An explicit named opt-in distinguishes them from Qt internals.
            return false;
        }
        if (!action)
        {
            return true;
        }

        // Qt installs implementation actions on value widgets (for example
        // QLineEdit's clear action and spin-box editing commands).  They are
        // not editor commands and must never leak into the user keymap.
        const QByteArray className = (action->metaObject()
            ? QByteArray(action->metaObject()->className())
            : QByteArray{}).toLower();
        if (className.contains("qlineedit")
            || className.contains("spinbox")
            || className.contains("combobox")
            || className.contains("clearaction"))
        {
            return true;
        }

        // A few Qt styles parent implementation actions to a text-control or
        // popup helper rather than directly to the visible editor.  Walk the
        // owner chain so those transient actions are filtered as well.
        for (const QObject* owner = action->parent(); owner; owner = owner->parent())
        {
            if (qobject_cast<const QLineEdit*>(owner)
                || qobject_cast<const QComboBox*>(owner)
                || qobject_cast<const QAbstractSpinBox*>(owner)
                || qobject_cast<const QAbstractScrollArea*>(owner)
                || qobject_cast<const QAbstractItemView*>(owner))
            {
                return true;
            }
            const QMetaObject* meta = owner->metaObject();
            const QByteArray ownerClass = (meta
                ? QByteArray(meta->className()) : QByteArray{}).toLower();
            if (ownerClass.contains("qlineedit")
                || ownerClass.contains("spinbox")
                || ownerClass.contains("combobox")
                || ownerClass.contains("clearaction")
                || ownerClass.contains("textcontrol"))
            {
                return true;
            }
        }
        return false;
    };

    auto humanizeBindingToken = [](QString token) -> QString
    {
        token.replace(QLatin1Char('_'), QLatin1Char(' '));
        token.replace(QLatin1Char('-'), QLatin1Char(' '));
        token = token.simplified();
        bool capitalize = true;
        for (qsizetype i = 0; i < token.size(); ++i)
        {
            QChar ch = token.at(i);
            if (capitalize && ch.isLetter())
            {
                token[i] = ch.toUpper();
                capitalize = false;
            }
            else if (ch.isSpace())
            {
                capitalize = true;
            }
        }
        return token;
    };

    auto friendlyBindingLabel = [actionLabelForKeymap, humanizeBindingToken](
        const QAction* action,
        const QString& actionId) -> QString
    {
        if ( actionId == QStringLiteral("viewport.show_grid") )
        {
            return QStringLiteral("Toggle Grid");
        }
        if ( actionId == QStringLiteral("viewport.map.grid_rotate_to_view") )
        {
            return QStringLiteral("Always Align Grid to View");
        }
        QString label = actionLabelForKeymap(action);
        if (!label.isEmpty()
            && label.compare(actionId, Qt::CaseInsensitive) != 0
            && !label.contains(QLatin1Char('.')))
        {
            return label;
        }

        QStringList parts = actionId.split(QLatin1Char('.'), Qt::SkipEmptyParts);
        if (parts.isEmpty())
        {
            return QStringLiteral("Action");
        }
        return humanizeBindingToken(parts.back());
    };

    auto bindingCategoryPath = [](const QString& actionId) -> QStringList
    {
        const QString id = actionId.toLower();
        if (id.startsWith(QStringLiteral("cleave.transform.")))
        {
            return { QStringLiteral("Editing"), QStringLiteral("Cleave"), QStringLiteral("Keyboard") };
        }
        if (id.startsWith(QStringLiteral("viewport.fly."))
            || id.startsWith(QStringLiteral("viewport.zoom_"))
            || id.startsWith(QStringLiteral("viewport.orbit_"))
            || id.startsWith(QStringLiteral("viewport.pan_"))
            || id.startsWith(QStringLiteral("viewport.roll_"))
            || id.startsWith(QStringLiteral("viewport.view_"))
            || id == QStringLiteral("viewport.frame_level")
            || id == QStringLiteral("viewport.toggle_projection"))
        {
            return { QStringLiteral("Viewport"), QStringLiteral("Camera") };
        }
        if (id.startsWith(QStringLiteral("viewport.render"))
            || id.startsWith(QStringLiteral("viewport.lighting."))
            || id.startsWith(QStringLiteral("viewport.things."))
            || id.startsWith(QStringLiteral("viewport.audio."))
            || id.startsWith(QStringLiteral("viewport.map."))
            || id.startsWith(QStringLiteral("viewport.grid_"))
            || id.startsWith(QStringLiteral("viewport.xray_"))
            || id.startsWith(QStringLiteral("viewport.gizmos."))
            || id.startsWith(QStringLiteral("viewport.show_"))
            || id == QStringLiteral("viewport.show_grid"))
        {
            return { QStringLiteral("Viewport"), QStringLiteral("Display") };
        }
        if (id.startsWith(QStringLiteral("viewport.transform."))
            || id.startsWith(QStringLiteral("editor.tool."))
            || id.startsWith(QStringLiteral("editor.transform."))
            || id.startsWith(QStringLiteral("thing_move."))
            || id.startsWith(QStringLiteral("thing_placement."))
            || id.startsWith(QStringLiteral("sector_move."))
            || id.startsWith(QStringLiteral("transform.modifier.")))
        {
            return { QStringLiteral("Viewport"), QStringLiteral("Transform and Placement") };
        }
        if (id.startsWith(QStringLiteral("viewport.sector."))
            || id.startsWith(QStringLiteral("viewport.surface."))
            || id.startsWith(QStringLiteral("viewport.frame."))
            || id.startsWith(QStringLiteral("viewport.light_"))
            || id.startsWith(QStringLiteral("light.command."))
            || id.startsWith(QStringLiteral("light.submode.")))
        {
            return { QStringLiteral("Viewport"), QStringLiteral("Tools") };
        }
        if (id.startsWith(QStringLiteral("surface.uv.")))
        {
            return { QStringLiteral("Editors"), QStringLiteral("UV Editor") };
        }
        if (id.startsWith(QStringLiteral("cog_editor.")))
        {
            return { QStringLiteral("Editors"), QStringLiteral("COG Script Editor") };
        }
        if (id.startsWith(QStringLiteral("selection.")))
        {
            return { QStringLiteral("Editing"), QStringLiteral("Selection") };
        }
        if (id.startsWith(QStringLiteral("edit_mode."))
            || id.startsWith(QStringLiteral("surface.submode.")))
        {
            return { QStringLiteral("Editing"), QStringLiteral("Modes") };
        }
        if (id.startsWith(QStringLiteral("sector.command."))
            || id.startsWith(QStringLiteral("surface.command."))
            || id.startsWith(QStringLiteral("thing.command."))
            || id.startsWith(QStringLiteral("sector."))
            || id.startsWith(QStringLiteral("surface."))
            || id.startsWith(QStringLiteral("thing.")))
        {
            return { QStringLiteral("Editing"), QStringLiteral("Entity Commands") };
        }
        if (id.startsWith(QStringLiteral("transform.")))
        {
            return { QStringLiteral("Editing"), QStringLiteral("Transform") };
        }
        if (id.startsWith(QStringLiteral("file.")))
        {
            return { QStringLiteral("Application"), QStringLiteral("File") };
        }
        if (id.startsWith(QStringLiteral("edit.")))
        {
            return { QStringLiteral("Application"), QStringLiteral("Edit") };
        }
        return { QStringLiteral("Application"), QStringLiteral("Other") };
    };

    QHash<QString, QTreeWidgetItem*> keymapCategoryItems{};
    auto categoryItemForPath = [keymapTree, &keymapCategoryItems](
        const QStringList& path) -> QTreeWidgetItem*
    {
        QTreeWidgetItem* parentItem = nullptr;
        QString key{};
        for (const QString& component : path)
        {
            if (!key.isEmpty()) key += QLatin1Char('/');
            key += component;
            QTreeWidgetItem* item = keymapCategoryItems.value(key, nullptr);
            if (!item)
            {
                const bool topLevel = parentItem == nullptr;
                item = parentItem
                    ? new PreferenceKeymapItem(parentItem)
                    : new PreferenceKeymapItem(keymapTree);
                item->setText(0, component);
                item->setFirstColumnSpanned(true);
                item->setFlags(Qt::ItemIsEnabled);
                // Category rows are deliberately real tree rows rather than
                // plain text labels.  Give them a slightly stronger band and
                // a little vertical breathing room so the action rows remain
                // visually grouped when many bindings are expanded; the
                // alternating/category shades are intentionally line-free.
                item->setData(0, Qt::BackgroundRole,
                    QColor(topLevel ? 42 : 48, topLevel ? 47 : 53,
                           topLevel ? 58 : 63));
                item->setSizeHint(0, QSize(-1, topLevel ? 26 : 23));
                QFont font = item->font(0);
                font.setBold(true);
                item->setFont(0, font);
                item->setExpanded(true);
                keymapCategoryItems.insert(key, item);
            }
            parentItem = item;
        }
        return parentItem;
    };

    auto actionShortcutsToStrings = [](const QList<QKeySequence>& shortcuts) -> QStringList
    {
        QStringList out{};
        out.reserve(shortcuts.size());
        for (const QKeySequence& seq : shortcuts)
        {
            const QString portable = seq.toString(QKeySequence::PortableText).trimmed();
            if (!portable.isEmpty())
            {
                out.push_back(portable);
            }
        }
        return out;
    };

    QList<QAction*> keymapActions = findChildren<QAction*>();
    keymapActions.erase(
        std::remove_if(
            keymapActions.begin(),
            keymapActions.end(),
            [](QAction* action)
            {
                // Menu containers, separators, and embedded popup controls are
                // presentation plumbing, not executable user commands.
                return !action || action->isSeparator() || action->menu()
                    || qobject_cast<QWidgetAction*>(action);
            }),
        keymapActions.end());
    keymapActions.erase(
        std::remove_if(
            keymapActions.begin(),
            keymapActions.end(),
            [isInternalWidgetAction](QAction* action)
            {
                if (isInternalWidgetAction(action))
                {
                    return true;
                }

                // Anonymous empty actions are intentionally omitted.  This
                // removes transient menu/panel entries such as "At View",
                // axis labels, dock positions, and query-field labels while
                // retaining commands with a user binding/default or an
                // explicit opt-in and cleared bindings through their saved
                // defaults. Project-layer visibility/lock actions are an
                // intentional exception: their QAction instances are
                // rebuilt lazily by the Map menus, so exposing them here
                // would show stale Layer0 rows or rows from a previous
                // document. Layer controls remain available in Map menus,
                // but are not user-keymap entries.
                const QString layerName = action
                    ? (action->property("sed.layerName").toString().trimmed().isEmpty()
                        ? action->property("sed_layer_name").toString().trimmed()
                        : action->property("sed.layerName").toString().trimmed())
                    : QString{};
                if (!layerName.isEmpty())
                {
                    return true;
                }
                const bool hasCurrentShortcut = action && !action->shortcuts().isEmpty();
                const bool hasDefaultShortcut = action
                    && !action->property("sed_default_shortcuts").toStringList().isEmpty();
                // An object name is an implementation identifier, not an
                // invitation to expose the action.  Many transient placement
                // controls (axis buttons and At View) have stable names but
                // no user binding; only a current/default shortcut or an
                // explicit opt-in may enter the keymap.
                const bool explicitlyExposed = action
                    && action->property("sed_keymap_expose").toBool();
                return !hasCurrentShortcut && !hasDefaultShortcut
                    && !explicitlyExposed;
            }),
        keymapActions.end());
    for (QAction* action : keymapActions)
    {
        (void)keymapIdForAction(action);
    }
    std::sort(keymapActions.begin(), keymapActions.end(), [&friendlyBindingLabel, &keymapIdForAction](QAction* a, QAction* b)
        {
            const QString aName = friendlyBindingLabel(a, keymapIdForAction(a));
            const QString bName = friendlyBindingLabel(b, keymapIdForAction(b));
            const int cmp = QString::compare(aName, bName, Qt::CaseInsensitive);
            if (cmp != 0)
            {
                return cmp < 0;
            }
            const QString aId = a ? (a->objectName().trimmed().isEmpty()
                                         ? a->property("sed_generated_keymap_id").toString().trimmed()
                                         : a->objectName().trimmed())
                                  : QString();
            const QString bId = b ? (b->objectName().trimmed().isEmpty()
                                         ? b->property("sed_generated_keymap_id").toString().trimmed()
                                         : b->objectName().trimmed())
                                  : QString();
            return QString::compare(aId, bId, Qt::CaseInsensitive) < 0;
        });

    {
        QSettings keymapSettings = settingsStore();
        keymapSettings.beginGroup("keymap/shortcuts");

        for (QAction* action : keymapActions)
        {
            if (!action)
            {
                continue;
            }

            const QString actionId = keymapIdForAction(action);
            if (actionId.isEmpty())
            {
                continue;
            }

            if (!action->property("sed_default_shortcuts").isValid())
            {
                action->setProperty("sed_default_shortcuts", actionShortcutsToStrings(action->shortcuts()));
            }

            QStringList configured = actionShortcutsToStrings(action->shortcuts());
            if ( keymapSettings.contains(actionId) )
            {
                configured = keymapSettings.value(actionId).toStringList();
            }
            if ( !keymapActionsById.emplace(actionId.toStdString(), action).second )
            {
                continue;
            }
            const QString label = friendlyBindingLabel(action, actionId);
            const QString rawDescription = !action->toolTip().trimmed().isEmpty()
                ? action->toolTip().trimmed() : action->statusTip().trimmed();
            const QString description = rawDescription.isEmpty()
                ? QStringLiteral("Run %1.").arg(label) : rawDescription;
            const QString helpText = QStringLiteral("%1\nBinding ID: %2").arg(description, actionId);
            auto* const item = new PreferenceKeymapItem(categoryItemForPath(bindingCategoryPath(actionId)));
            item->setText(0, label);
            item->setToolTip(0, helpText);
            QList<QKeySequence> alternatives{};
            for ( const QString& text : configured )
            {
                alternatives.push_back(QKeySequence(text, QKeySequence::PortableText));
            }
            auto* const controls = new PreferenceShortcutAlternatives(alternatives,
                action->property("sed_modifier_only_shortcut").toBool(), helpText, keymapTree);
            controls->setFont(keymapTree->font());
            const int rowHeight = std::max(26, controls->fontMetrics().height() + 10) + 4;
            item->setSizeHint(0, QSize(0, rowHeight));
            item->setSizeHint(1, QSize(0, rowHeight));
            keymapTree->setItemWidget(item, 1, controls);
            keymapRows.push_back(KeymapEditRow{
                actionId, item, controls,
                QStringLiteral("%1 %2 %3").arg(label, description, actionId).toLower() });
        }

        keymapSettings.endGroup();
    }

    const auto addCleaveGesture = [&](const char* id, const QString& label,
                                      const QString& gesture, const QString& context)
    {
        const QString actionId = QString::fromLatin1(id);
        auto* item = new PreferenceKeymapItem(categoryItemForPath(
            {QStringLiteral("Editing"), QStringLiteral("Cleave"), QStringLiteral("Mouse gestures")}));
        item->setText(0, label);
        item->setText(1, gesture);
        item->setToolTip(0, QStringLiteral("%1\nFixed mouse gesture.\nBinding ID: %2").arg(context, actionId));
        item->setToolTip(1, item->toolTip(0));
        item->setSizeHint(0, QSize(0, std::max(26, keymapTree->fontMetrics().height() + 10)));
        keymapRows.push_back(KeymapEditRow{actionId, item, nullptr,
            QStringLiteral("%1 %2 %3").arg(label, context, actionId).toLower()});
    };
    addCleaveGesture("cleave.gesture.shape_scale", QStringLiteral("Shape scale"), QStringLiteral("Ctrl Wheel"),
        QStringLiteral("Resize the armed shape uniformly over the active Multi-cut viewport; dimensions retain their proportions."));
    addCleaveGesture("cleave.gesture.shape_rotate", QStringLiteral("Shape rotate"), QStringLiteral("Ctrl Shift Wheel"),
        QStringLiteral("Rotate the armed shape over the active Multi-cut viewport by Angle step, or 5 degrees when Free is selected."));
    addCleaveGesture("cleave.gesture.group_rotate", QStringLiteral("Selected cuts rotate"), QStringLiteral("Ctrl Wheel"),
        QStringLiteral("Rotate selected cuts while the local Rotate mode is active; use Angle step, or 5 degrees when Free is selected."));
    addCleaveGesture("cleave.gesture.group_scale", QStringLiteral("Selected cuts scale"), QStringLiteral("Ctrl Wheel"),
        QStringLiteral("Uniformly scale selected cuts while the local Scale mode is active."));
    addCleaveGesture("cleave.gesture.geometry_snap", QStringLiteral("Geometry snap"), QStringLiteral("Shift Drag"),
        QStringLiteral("Snap a Cleave construction point or endpoint to nearby compatible geometry."));
    addCleaveGesture("cleave.gesture.grid_invert", QStringLiteral("Grid snap invert"), QStringLiteral("Ctrl Drag"),
        QStringLiteral("Temporarily reverse Grid Snap while positioning the armed shape or an endpoint."));
    addCleaveGesture("cleave.gesture.angle_snap", QStringLiteral("Endpoint angle snap"), QStringLiteral("Ctrl Shift Drag"),
        QStringLiteral("Snap an edited endpoint to Angle step; dragging a whole line uses its construction plane."));
    addCleaveGesture("cleave.gesture.detach", QStringLiteral("Detach joined cuts"), QStringLiteral("Alt Drag"),
        QStringLiteral("Detach a selected Cleave endpoint or line from its joined neighbor."));
    addCleaveGesture("cleave.gesture.selection_add", QStringLiteral("Add cut selection"), QStringLiteral("Shift Click"),
        QStringLiteral("Add a staged Cleave line to the selection."));
    addCleaveGesture("cleave.gesture.selection_remove", QStringLiteral("Remove cut selection"), QStringLiteral("Ctrl Click"),
        QStringLiteral("Remove a staged Cleave line from the selection."));
    addCleaveGesture("cleave.gesture.cycle", QStringLiteral("Cycle coincident cuts"), QStringLiteral("Alt Click"),
        QStringLiteral("Cycle through staged cuts at the same pointer location."));
    addCleaveGesture("cleave.gesture.marquee_add", QStringLiteral("Add marquee cuts"), QStringLiteral("Alt Drag empty space"),
        QStringLiteral("Add staged Cleave lines intersecting a marquee to the selection."));
    addCleaveGesture("cleave.gesture.marquee_remove", QStringLiteral("Remove marquee cuts"), QStringLiteral("Alt Ctrl Drag empty space"),
        QStringLiteral("Remove staged Cleave lines inside a marquee from the selection."));

    // Sort the actual displayed labels after every category and action exists.
    // Qt keeps the row widgets and persistent item ownership during this sort.
    keymapTree->sortItems(0, Qt::AscendingOrder);

    auto applyKeymapFilter = [keymapSearch, keymapTree, &keymapRows]()
    {
        const QString needle = keymapSearch->text().trimmed().toLower();
        QHash<QTreeWidgetItem*, bool> matchedItems{};
        for (const KeymapEditRow& row : keymapRows)
        {
            if (!row.item)
            {
                continue;
            }

            QStringList shortcutText{};
            if ( row.controls )
            {
                for ( const QKeySequence& sequence : row.controls->shortcuts() )
                {
                    shortcutText.push_back(ui::shortcuts::keySequenceDisplayText(sequence));
                }
            }
            const QString keyText = row.controls
                ? shortcutText.join(QLatin1Char(' ')).toLower() : row.item->text(1).toLower();
            const bool visible = needle.isEmpty()
                || row.searchText.contains(needle)
                || keyText.contains(needle);
            matchedItems[row.item] = matchedItems.value(row.item, false) || visible;
        }
        for ( auto item = matchedItems.cbegin(); item != matchedItems.cend(); ++item )
        {
            item.key()->setHidden(!item.value());
        }
        auto updateCategoryVisibility = [](auto&& self, QTreeWidgetItem* item) -> bool
        {
            bool anyVisible = false;
            for (int i = 0; item && i < item->childCount(); ++i)
            {
                QTreeWidgetItem* child = item->child(i);
                const bool childVisible = child->childCount() > 0
                    ? self(self, child)
                    : !child->isHidden();
                anyVisible = anyVisible || childVisible;
            }
            if (item && item->childCount() > 0)
            {
                item->setHidden(!anyVisible);
            }
            return anyVisible;
        };
        for (int i = 0; i < keymapTree->topLevelItemCount(); ++i)
        {
            updateCategoryVisibility(updateCategoryVisibility, keymapTree->topLevelItem(i));
        }
    };

    QObject::connect(keymapSearch, &QLineEdit::textChanged, &dialog, [applyKeymapFilter](const QString&)
        {
            applyKeymapFilter();
        });

    const auto validateKeymapControls = [&]()
    {
        QList<ui::shortcuts::KeymapShortcutBinding> bindings{};
        for ( const KeymapEditRow& row : keymapRows )
        {
            if ( !row.controls )
            {
                continue;
            }
            const auto action = keymapActionsById.find(row.actionId.toStdString());
            if ( action != keymapActionsById.end() && action->second )
            {
                bindings.push_back({action->second, row.item->text(0), row.controls->shortcuts()});
            }
        }
        const QString error = ui::shortcuts::keymapShortcutConflict(bindings);
        keymapValid = error.isEmpty();
        keymapError->setText(error);
        keymapError->setVisible(!keymapValid);
        updatePreferenceAcceptState();
        return keymapValid;
    };
    for ( const KeymapEditRow& row : keymapRows )
    {
        if ( row.controls )
        {
            row.controls->setChangeHandler([applyKeymapFilter, validateKeymapControls]()
            {
                applyKeymapFilter();
                (void)validateKeymapControls();
            });
        }
    }
    QObject::connect(keymapResetDefaults, &QPushButton::clicked, &dialog,
        [&keymapRows, &keymapActionsById, applyKeymapFilter, validateKeymapControls]()
    {
        for ( const KeymapEditRow& row : keymapRows )
        {
            if ( !row.controls )
            {
                continue;
            }
            QList<QKeySequence> defaults{};
            const auto action = keymapActionsById.find(row.actionId.toStdString());
            if ( action != keymapActionsById.end() && action->second )
            {
                for ( const QString& value : action->second->property("sed_default_shortcuts").toStringList() )
                {
                    defaults.push_back(QKeySequence(value, QKeySequence::PortableText));
                }
            }
            const QSignalBlocker blocker(row.controls);
            row.controls->setShortcuts(defaults);
        }
        applyKeymapFilter();
        (void)validateKeymapControls();
    });

    applyKeymapFilter();

    categories->addItem("Keymap");
    stack->addWidget(keymapPage);
    QTreeWidgetItem* cameraZoomInTarget{};
    QTreeWidgetItem* cameraZoomOutTarget{};
    QTreeWidgetItem* cameraFlySpeedUpTarget{};
    QTreeWidgetItem* cameraFlySpeedDownTarget{};
    for ( const KeymapEditRow& row : keymapRows )
    {
        if ( row.actionId == QStringLiteral("viewport.zoom_in") )
        {
            cameraZoomInTarget = row.item;
        }
        else if ( row.actionId == QStringLiteral("viewport.zoom_out") )
        {
            cameraZoomOutTarget = row.item;
        }
        else if ( row.actionId == QStringLiteral("viewport.fly.speed_up") )
        {
            cameraFlySpeedUpTarget = row.item;
        }
        else if ( row.actionId == QStringLiteral("viewport.fly.speed_down") )
        {
            cameraFlySpeedDownTarget = row.item;
        }
    }
    QTreeWidgetItem* const cameraCategoryTarget =
        keymapCategoryItems.value(QStringLiteral("Viewport/Camera"), nullptr);
    auto* const cameraLinkHighlightTimer = new QTimer(keymapTree);
    cameraLinkHighlightTimer->setObjectName(
        QStringLiteral("pref_keymap_link_highlight_timer"));
    cameraLinkHighlightTimer->setSingleShot(true);
    std::array<QTreeWidgetItem*, 2> highlightedCameraRows{};
    const auto clearCameraLinkHighlight = [&]()
    {
        cameraLinkHighlightTimer->stop();
        for ( QTreeWidgetItem* const item : highlightedCameraRows )
        {
            if ( !item )
            {
                continue;
            }
            for ( int column = 0; column < keymapTree->columnCount(); ++column )
            {
                item->setData(column, kCameraKeymapLinkHighlightRole, false);
            }
        }
        highlightedCameraRows = {};
        keymapTree->viewport()->update();
    };
    QObject::connect(cameraLinkHighlightTimer, &QTimer::timeout, keymapTree,
        [keymapTree, &highlightedCameraRows]()
        {
            for ( QTreeWidgetItem* const item : highlightedCameraRows )
            {
                if ( !item )
                {
                    continue;
                }
                for ( int column = 0; column < keymapTree->columnCount(); ++column )
                {
                    item->setData(column, kCameraKeymapLinkHighlightRole, false);
                }
            }
            highlightedCameraRows = {};
            keymapTree->viewport()->update();
        });
    QObject::connect(wheelKeyboardHint, &QLabel::linkActivated, &dialog,
            [categories, stack, keymapPage, keymapSearch, keymapTree,
            cameraCategoryTarget,
            cameraZoomInTarget, cameraZoomOutTarget,
            cameraFlySpeedUpTarget, cameraFlySpeedDownTarget,
            cameraLinkHighlightTimer, &highlightedCameraRows,
            clearCameraLinkHighlight](const QString& target)
        {
            std::array<QTreeWidgetItem*, 2> cameraPair{};
            if ( target == QStringLiteral("#camera-zoom") )
            {
                cameraPair = { cameraZoomInTarget, cameraZoomOutTarget };
            }
            else if ( target == QStringLiteral("#camera-fly-speed") )
            {
                cameraPair = { cameraFlySpeedUpTarget, cameraFlySpeedDownTarget };
            }
            else
            {
                return;
            }
            clearCameraLinkHighlight();
            keymapSearch->clear();
            categories->setCurrentRow(stack->indexOf(keymapPage));
            QTreeWidgetItem* const cameraTarget = cameraPair.front()
                ? cameraPair.front()
                : cameraCategoryTarget;
            for ( QTreeWidgetItem* item = cameraTarget; item; item = item->parent() )
            {
                item->setExpanded(true);
            }
            if ( cameraTarget )
            {
                keymapTree->setCurrentItem(cameraTarget);
                QTreeWidgetItem* visibleTarget = cameraTarget;
                if ( cameraPair[0] && cameraPair[1]
                    && cameraPair[0]->parent()
                    && cameraPair[0]->parent() == cameraPair[1]->parent()
                    && cameraPair[0]->parent()->indexOfChild(cameraPair[1])
                        < cameraPair[0]->parent()->indexOfChild(cameraPair[0]) )
                {
                    visibleTarget = cameraPair[1];
                }
                keymapTree->scrollToItem(
                    visibleTarget, QAbstractItemView::PositionAtCenter);
            }
            keymapTree->setFocus(Qt::OtherFocusReason);
            if ( cameraPair[0] && cameraPair[1] )
            {
                highlightedCameraRows = cameraPair;
                for ( QTreeWidgetItem* const item : cameraPair )
                {
                    for ( int column = 0; column < keymapTree->columnCount(); ++column )
                    {
                        item->setData(column, kCameraKeymapLinkHighlightRole, true);
                    }
                }
                keymapTree->viewport()->update();
                cameraLinkHighlightTimer->start(500);
            }
        });

    // ---- Save and crash-recovery preferences page ----
    auto* recoveryPage = new QWidget(&dialog);
    recoveryPage->setProperty(
        "sed.preferencesDisclosurePath", QStringLiteral("save-recovery"));
    auto* recoveryLayout = new QVBoxLayout(recoveryPage);
    recoveryLayout->setContentsMargins(0, 0, 0, 0);
    recoveryLayout->setSpacing(12);

    auto* legacyUvSection = createCollapsedSection(
        QStringLiteral("legacy-uv"),
        "Legacy Level UVs", recoveryPage, true);
    auto* legacyUvForm = new QFormLayout();
    legacyUvForm->setContentsMargins(6, 4, 6, 4);
    legacyUvForm->setSpacing(8);

    auto* legacyUvPolicy = new QComboBox(legacyUvSection);
    legacyUvPolicy->setObjectName(
        QStringLiteral("pref_legacy_uv_promotion_policy"));
    legacyUvPolicy->addItem(
        QStringLiteral("Normalize automatically when possible"),
        static_cast<int>(LegacyUvPromotionPolicy::normalizeAutomatically));
    legacyUvPolicy->addItem(
        QStringLiteral("Ask before normalizing"),
        static_cast<int>(LegacyUvPromotionPolicy::askBeforeNormalizing));
    legacyUvPolicy->addItem(
        QStringLiteral("Keep legacy pixel space"),
        static_cast<int>(LegacyUvPromotionPolicy::keepPixelSpace));
    const int legacyUvPolicyIndex = legacyUvPolicy->findData(
        static_cast<int>(loadLegacyUvPromotionPolicy()));
    legacyUvPolicy->setCurrentIndex(std::max(0, legacyUvPolicyIndex));
    legacyUvPolicy->setToolTip(
        "SED V1 and V4 store UVs in texture pixels. Normalization preserves "
        "the visible mapping and converts the complete Level plus undo/redo "
        "history only when every required material size is available.");
    legacyUvForm->addRow("Legacy UV handling", legacyUvPolicy);

    auto* legacyUvExplanation = new QLabel(
        "Automatic normalization is recommended. If any material size is "
        "missing or invalid, SED leaves the complete document in legacy "
        "pixel space and reports which surface prevented conversion. The "
        "status bar shows when delayed resource availability makes the "
        "complete document ready to normalize.",
        legacyUvSection);
    legacyUvExplanation->setWordWrap(true);
    legacyUvExplanation->setStyleSheet(
        "color: rgba(220, 220, 220, 180);");
    legacyUvForm->addRow(legacyUvExplanation);
    legacyUvSection->contentLayout()->addLayout(legacyUvForm);
    recoveryLayout->addWidget(legacyUvSection);

    auto* recoverySection = createCollapsedSection(
        QStringLiteral("autosave-recovery"),
        "Autosave Recovery", recoveryPage, true);
    auto* recoveryForm = new QFormLayout();
    recoveryForm->setContentsMargins(6, 4, 6, 4);
    recoveryForm->setSpacing(8);

    const QSettings recoverySettings = settingsStore();
    auto* recoveryEnabledCheck = new QCheckBox(
        "Keep crash- and power-loss recovery snapshots", recoverySection);
    recoveryEnabledCheck->setObjectName("pref_recovery_autosave_enabled");
    recoveryEnabledCheck->setChecked(
        recoverySettings.value("recovery/autosave_enabled", true).toBool());
    recoveryForm->addRow("Autosave", recoveryEnabledCheck);

    auto* recoveryIntervalSpin = new QSpinBox(recoverySection);
    recoveryIntervalSpin->setObjectName("pref_recovery_autosave_interval");
    recoveryIntervalSpin->setRange(1, 30);
    recoveryIntervalSpin->setSuffix(" min");
    recoveryIntervalSpin->setValue(std::clamp(
        recoverySettings.value("recovery/autosave_interval_minutes", 5).toInt(),
        1,
        30));
    recoveryIntervalSpin->setEnabled(recoveryEnabledCheck->isChecked());
    recoveryIntervalSpin->setToolTip(
        "Changed Level or editor state is captured in the background. Up to 10 earlier autosaves per editing session are retained.");
    recoveryForm->addRow("Interval", recoveryIntervalSpin);
    QObject::connect(recoveryEnabledCheck, &QCheckBox::toggled,
        recoveryIntervalSpin, &QWidget::setEnabled);

    auto* recoveryExplanation = new QLabel(
        "SED stores a self-contained editor-format snapshot without overwriting the level. "
        "It offers unsaved snapshots on launch or before opening the matching level. "
        "A Save publishes a clean hard-kill baseline; clean close removes it. Earlier restore points are available from File > Open Recent > Autosave History.",
        recoverySection);
    recoveryExplanation->setWordWrap(true);
    recoveryExplanation->setStyleSheet("color: rgba(220, 220, 220, 180);");
    recoveryForm->addRow(recoveryExplanation);

    recoverySection->contentLayout()->addLayout(recoveryForm);
    recoveryLayout->addWidget(recoverySection);
    recoveryLayout->addStretch(1);

    categories->addItem("Save & Recovery");
    stack->addWidget(recoveryPage);

    QObject::connect(categories, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
    QObject::connect(categories, &QListWidget::currentRowChanged, &dialog, [this](const int row)
        {
            prefsLastCategoryRow_ = row;
        });

    const int initialRow = std::clamp(prefsLastCategoryRow_, 0, std::max(0, categories->count() - 1));
    categories->setCurrentRow(initialRow);

    const auto captureCogAppearanceControls = [&dialog]() -> QVariantList
    {
        QVariantList values{};
        if (const auto* combo = dialog.findChild<QComboBox*>(
                QStringLiteral("pref_cog_editor_font_family")))
        {
            values.push_back(combo->currentText().trimmed());
        }
        if (const auto* spin = dialog.findChild<QSpinBox*>(
                QStringLiteral("pref_cog_editor_font_size")))
        {
            values.push_back(spin->value());
        }
        for (const QString& objectName : {
                 QStringLiteral("pref_cog_editor_font_bold"),
                 QStringLiteral("pref_cog_editor_font_italic"),
                 QStringLiteral("pref_cog_editor_show_whitespace") })
        {
            if (const auto* check = dialog.findChild<QCheckBox*>(objectName))
            {
                values.push_back(check->isChecked());
            }
        }
        if (const auto* combo = dialog.findChild<QComboBox*>(
                QStringLiteral("pref_cog_editor_theme")))
        {
            values.push_back(combo->currentData());
            values.push_back(combo->property("sed.customThemeData"));
        }
        for (const CogScriptThemeColorDef& def : cogScriptThemeColorDefs())
        {
            if (const auto* button = dialog.findChild<QToolButton*>(
                    QStringLiteral("pref_cog_editor_color_%1").arg(def.key)))
            {
                values.push_back(button->property("sed.colorValue"));
            }
        }
        return values;
    };
    const QVariantList originalCogAppearanceControls =
        captureCogAppearanceControls();

    const auto captureViewportBannerControls = [&]()
    {
        ViewportBannerSettings result = originalViewportBannerSettings;
        for ( std::size_t index = 0; index < kViewportBannerKindCount; ++index )
        {
            const QString prefix = QStringLiteral("pref_banner_%1_")
                .arg(QString::fromLatin1(kViewportBannerKindIds[index]));
            if ( const auto* const duration =
                    dialog.findChild<QDoubleSpinBox*>(prefix + "hide_time") )
            {
                result.styles[index].hideAfterMs =
                    qRound(duration->value() * 1000.0);
            }
            if ( const auto* const useMessageDuration =
                    dialog.findChild<QCheckBox*>(prefix + "use_message_duration") )
            {
                result.styles[index].useMessageDuration =
                    useMessageDuration->isChecked();
            }
            const std::array<const char*, 3> colorFields{
                "text_color", "background_color", "border_color",
            };
            std::array<QColor*, 3> destinations{
                &result.styles[index].textColor,
                &result.styles[index].backgroundColor,
                &result.styles[index].borderColor,
            };
            for ( std::size_t colorIndex = 0; colorIndex < colorFields.size(); ++colorIndex )
            {
                const auto* const swatch = dialog.findChild<QToolButton*>(
                    prefix + QString::fromLatin1(colorFields[colorIndex]));
                if ( swatch )
                {
                    const QColor value = swatch->property("sed.colorValue").value<QColor>();
                    if ( value.isValid() )
                    {
                        *destinations[colorIndex] = value;
                    }
                }
            }
        }
        return result;
    };

    // Capture only controls which publish user configuration. Navigation,
    // search text and disclosure state intentionally stay outside this
    // fingerprint: they are transient dialog/session UI and must not turn an
    // otherwise unchanged Accept into a full settings rewrite. Local color
    // variables are included because most swatches do not encode their value
    // in a standard Qt value property.
    const auto capturePreferenceControls = [&]() -> QVariantList
    {
        QVariantList values{};
        for (const QCheckBox* check : dialog.findChildren<QCheckBox*>())
        {
            if ( check->property("sed.mouseNavigationPreference").toBool()
                || check->property("sed.viewportBannerPreference").toBool() )
            {
                continue;
            }
            values.push_back(check->isChecked());
        }
        for (const QSpinBox* spin : dialog.findChildren<QSpinBox*>())
        {
            if ( spin->property("sed.viewportBannerPreference").toBool() )
            {
                continue;
            }
            values.push_back(spin->value());
        }
        for (const QDoubleSpinBox* spin :
             dialog.findChildren<QDoubleSpinBox*>())
        {
            if ( spin->property("sed.viewportBannerPreference").toBool() )
            {
                continue;
            }
            values.push_back(spin->value());
        }
        for (const QComboBox* combo : dialog.findChildren<QComboBox*>())
        {
            if ( combo == decimalSeparator || combo->property("sed.mouseNavigationPreference").toBool()
                || combo->property("sed.viewportBannerPreference").toBool()
                || combo->property("sed.keymapAlternativeSelector").toBool() )
            {
                continue;
            }
            values.push_back(combo->currentIndex());
            values.push_back(combo->currentText());
            values.push_back(combo->currentData());
            values.push_back(combo->property("sed.customThemeData"));
        }
        for (const QLineEdit* pathEdit :
             dialog.findChildren<QLineEdit*>(
                 QStringLiteral("pref_path_edit")))
        {
            values.push_back(pathEdit->text());
        }
        for (const KeymapEditRow& row : keymapRows)
        {
            values.push_back(row.controls
                ? actionShortcutsToStrings(row.controls->shortcuts()) : QStringList{});
        }

        const auto appendColor = [&values](const QColor& color)
        {
            values.push_back(color.isValid()
                ? color.name(QColor::HexArgb) : QString{});
        };
        appendColor(iconCubeTint);
        appendColor(viewportBgClear);
        appendColor(viewportWireColor);
        appendColor(viewportGridColor);
        appendColor(viewportGridMajorColor);
        appendColor(viewportGridAxisXColor);
        appendColor(viewportGridAxisYColor);
        appendColor(viewportGridAxisZColor);
        appendColor(uvCanvasBackgroundColor);
        appendColor(uvGridMainColor);
        appendColor(uvGridSubColor);
        appendColor(uvGridAxisUColor);
        appendColor(uvGridAxisVColor);
        appendColor(viewportSelPrimary);
        appendColor(viewportSelMulti);
        appendColor(viewportSelEdgeMulti);
        appendColor(viewportSelActive);
        appendColor(viewportSelVertex);
        appendColor(viewportSelVertexMulti);
        appendColor(viewportCogPickPrimary);
        appendColor(viewportCogPickActive);
        appendColor(architecturalLightingMarkerColor);

        std::vector<std::string> typeColorKeys{};
        typeColorKeys.reserve(typeColors.size());
        for (const auto& [key, color] : typeColors)
        {
            (void)color;
            typeColorKeys.push_back(key);
        }
        std::sort(typeColorKeys.begin(), typeColorKeys.end());
        for (const std::string& key : typeColorKeys)
        {
            values.push_back(QString::fromStdString(key));
            appendColor(typeColors.at(key));
        }
        for (const QToolButton* button :
             dialog.findChildren<QToolButton*>())
        {
            if ( button->property("sed.viewportBannerPreference").toBool() )
            {
                continue;
            }
            const QVariant color = button->property("sed.colorValue");
            if (color.isValid())
            {
                values.push_back(button->objectName());
                appendColor(color.value<QColor>());
            }
        }
        return values;
    };
    const QVariantList originalPreferenceControls =
        capturePreferenceControls();

    const auto installDirsEquivalent = [](
        const std::optional<std::filesystem::path>& a,
        const std::optional<std::filesystem::path>& b)
    {
        if (!a || a->empty())
        {
            return !b || b->empty();
        }
        if (!b || b->empty())
        {
            return false;
        }
        const QString normalizedA = QString::fromStdWString(
            a->lexically_normal().wstring());
        const QString normalizedB = QString::fromStdWString(
            b->lexically_normal().wstring());
        return normalizedA.compare(normalizedB, Qt::CaseInsensitive) == 0
            || ui::resources::pathsEquivalentForSettings(*a, *b);
    };

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    (void)validateMouseNavigationControls();
    (void)validateKeymapControls();
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
        [&dialog, &rows, &installDirsEquivalent,
            &persistViewportThemeFiles, &persistCogThemeFiles,
            &viewportThemeFilesDirty, &cogThemeFilesDirty,
            &viewportThemeDraftPending,
            &customViewportColorThemes, &preflightCogThemeFiles,
            &originalViewportThemeFiles,
            &originalViewportThemeKeyPresent, &originalViewportThemeId,
            &validateMouseNavigationControls, &validateKeymapControls,
            &captureViewportBannerControls, &originalViewportBannerSettings]()
        {
            if ( !validateMouseNavigationControls() || !validateKeymapControls() )
            {
                return;
            }
            for ( const ProjectRow& row : rows )
            {
                const QString text = row.edit->text().trimmed();
                const std::optional<std::filesystem::path> installDir =
                    text.isEmpty()
                    ? std::nullopt
                    : std::optional<std::filesystem::path>(
                        std::filesystem::path(text.toStdWString()));
                if (installDirsEquivalent(
                        row.originalInstallDir, installDir)
                    || !installDir)
                {
                    continue;
                }

                const ui::resources::ProjectInstallPathValidation validation =
                    ui::resources::validateProjectInstallPath(
                        row.kind, *installDir);
                if (validation.canUse())
                {
                    continue;
                }

                row.edit->setFocus(Qt::OtherFocusReason);
                row.edit->selectAll();
                QMessageBox::warning(
                    &dialog,
                    QStringLiteral("Project Path Does Not Match"),
                    ui::project::projectInstallPathMismatchMessage(
                        row.kind, *installDir, validation));
                return;
            }
            if ( viewportThemeDraftPending )
            {
                QMessageBox::warning(
                    &dialog,
                    QStringLiteral("Viewport Theme Has Unsaved Color Edits"),
                    QStringLiteral("Click Update Theme to keep these color edits, or select another theme to discard them."));
                return;
            }

            const bool saveViewportThemes = viewportThemeFilesDirty;
            const bool saveCogThemes = cogThemeFilesDirty;
            QSettings themeSettings = settingsStore();
            QString themeSaveError{};
            if ( saveViewportThemes
                && !render::preflightCustomViewportColorThemes(
                    themeSettings, customViewportColorThemes, &themeSaveError))
            {
                QMessageBox::warning(
                    &dialog,
                    QStringLiteral("Could Not Save Viewport Themes"),
                    themeSaveError.isEmpty()
                        ? QStringLiteral("The viewport theme files could not be prepared for saving.")
                        : themeSaveError);
                return;
            }
            if ( saveCogThemes
                && !preflightCogThemeFiles(
                    themeSettings, &themeSaveError))
            {
                QMessageBox::warning(
                    &dialog,
                    QStringLiteral("Could Not Save COG Themes"),
                    themeSaveError.isEmpty()
                        ? QStringLiteral("The COG theme files could not be prepared for saving.")
                        : themeSaveError);
                return;
            }

            auto restoreViewportThemeSelection = [&]()
            {
                if ( originalViewportThemeKeyPresent )
                {
                    themeSettings.setValue(
                        QString::fromLatin1(render::kViewportColorThemeSetting),
                        originalViewportThemeId);
                }
                else
                {
                    themeSettings.remove(
                        QString::fromLatin1(render::kViewportColorThemeSetting));
                }
                themeSettings.sync();
                return themeSettings.status() == QSettings::NoError;
            };
            bool viewportThemesCommitted = false;
            if ( saveViewportThemes )
            {
                themeSaveError.clear();
                if ( !persistViewportThemeFiles(&themeSaveError) )
                {
                    QMessageBox::warning(
                        &dialog,
                        QStringLiteral("Could Not Save Viewport Themes"),
                        themeSaveError.isEmpty()
                            ? QStringLiteral("The viewport theme files could not be saved. The previous theme set was restored.")
                            : themeSaveError);
                    return;
                }
                viewportThemesCommitted = true;
            }
            if ( saveCogThemes )
            {
                themeSaveError.clear();
                if ( !persistCogThemeFiles(&themeSaveError) )
                {
                    if ( viewportThemesCommitted )
                    {
                        QString viewportRollbackError{};
                        const bool viewportRolledBack =
                            render::saveCustomViewportColorThemes(
                                themeSettings, originalViewportThemeFiles,
                                &viewportRollbackError);
                        const bool selectionRestored =
                            restoreViewportThemeSelection();
                        if ( !viewportRolledBack || !selectionRestored )
                        {
                            themeSaveError += QStringLiteral("\nViewport theme rollback was incomplete: %1")
                                .arg(viewportRollbackError.isEmpty()
                                    ? QStringLiteral("the original active theme could not be restored")
                                    : viewportRollbackError);
                        }
                    }
                    QMessageBox::warning(
                        &dialog,
                        QStringLiteral("Could Not Save COG Themes"),
                        themeSaveError.isEmpty()
                            ? QStringLiteral("The COG theme files could not be saved. The previous theme set was restored.")
                            : themeSaveError);
                    return;
                }
            }
            const ViewportBannerSettings chosenViewportBannerSettings =
                captureViewportBannerControls();
            if ( chosenViewportBannerSettings != originalViewportBannerSettings )
            {
                QSettings bannerSettings = settingsStore();
                if ( !saveViewportBannerSettings(
                        bannerSettings, chosenViewportBannerSettings) )
                {
                    QMessageBox::warning(
                        &dialog,
                        QStringLiteral("Could Not Save Viewport Banners"),
                        QStringLiteral("The viewport banner preferences could not be saved."));
                    return;
                }
            }
            dialog.accept();
        });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
    {
        // These three controls preview live in the viewport. Cancel restores
        // the exact pre-dialog state instead of leaving unpersisted changes.
        if (glView_)
        {
            glView_->setSurfaceNormalLengthScale(originalSurfaceNormalLength);
            glView_->setSurfaceNormalArrowheadScale(originalSurfaceNormalArrowhead);
            glView_->setShowSurfaceNormalArrowhead(
                originalShowSurfaceNormalArrowhead);
        }
        // COG font controls preview all existing tabs without changing their
        // source documents. Reload only if the live preview differs from the
        // opening state; an unrelated Cancel remains O(1) even with large COG
        // documents open, and each tab still retains its presentation zoom.
        if (cogFontPreviewChanged
            && captureCogAppearanceControls()
                != originalCogAppearanceControls)
        {
            refreshCogScriptEditorAppearance();
        }
        return;
    }

    const ViewportNavigationSettings chosenMouseNavigation = captureMouseNavigationControls();
    const bool mouseNavigationChanged = chosenMouseNavigation != originalMouseNavigation;
    const ViewportBannerSettings chosenViewportBannerSettings =
        captureViewportBannerControls();
    const bool viewportBannerChanged =
        chosenViewportBannerSettings != originalViewportBannerSettings;
    const bool decimalSeparatorChanged = decimalSeparator->currentData().toInt()
        != static_cast<int>(originalDecimalSeparator);
    const bool otherPreferencesChanged = capturePreferenceControls() != originalPreferenceControls
        || viewportThemeFilesDirty || cogThemeFilesDirty;
    if ( !decimalSeparatorChanged && !otherPreferencesChanged
        && !mouseNavigationChanged && !viewportBannerChanged )
    {
        return;
    }

    const qulonglong applyPublicationCount = property(
        "sed.preferencesApplyPublicationCount").toULongLong();
    setProperty(
        "sed.preferencesApplyPublicationCount",
        applyPublicationCount + 1u);

    if ( decimalSeparatorChanged )
    {
        const auto selectedSeparator = static_cast<UiDecimalSeparator>(
            decimalSeparator->currentData().toInt());
        saveUiDecimalSeparator(selectedSeparator);
        ui::applyUiDecimalSeparator(selectedSeparator);
    }
    if ( mouseNavigationChanged )
    {
        QSettings settings = settingsStore();
        if ( saveViewportNavigationSettings(settings, chosenMouseNavigation) && glView_ )
        {
            glView_->setViewportNavigationSettings(chosenMouseNavigation);
        }
    }
    if ( viewportBannerChanged )
    {
        if ( glView_ )
        {
            (void)glView_->setViewportBannerSettings(chosenViewportBannerSettings);
        }
    }
    if ( !otherPreferencesChanged )
    {
        return;
    }

    const bool cogAppearanceChanged =
        captureCogAppearanceControls() != originalCogAppearanceControls;
    bool layerViewportPreferencesChanged = false;
    bool layerOutlinerPreferenceChanged = false;

    bool activeResourceConfigurationChanged = false;
    for (const ProjectRow& row : rows)
    {
        const QString text = row.edit->text().trimmed();
        const std::optional<std::filesystem::path> installDir =
            text.isEmpty() ? std::nullopt : std::optional<std::filesystem::path>(std::filesystem::path(text.toStdWString()));

        if (installDirsEquivalent(row.originalInstallDir, installDir))
        {
            continue;
        }

        writeProjectInstallDirSetting(row.kind, installDir);
        activeResourceConfigurationChanged =
            activeResourceConfigurationChanged || row.kind == resourceKind_;
        // Discovery/mounting can traverse and index several large archives.
        // The active project is staged once by the worker below; an inactive
        // project's new install path is intentionally discovered when that
        // project is next opened. Keeping either scan out of this modal
        // accept path is what makes the dialog close immediately.
    }

    {
        QSettings settings = settingsStore();
        for (std::size_t project = 0;
             project < exportBufferProjects.size(); ++project)
        {
            LevelExportBufferExtras configured{};
            for (std::size_t row = 0;
                 row < exportBufferFields.size(); ++row)
            {
                configured.*(exportBufferFields[row].member) =
                    static_cast<std::uint32_t>(
                        exportBufferSpins[project][row]->value());
            }
            ui::project::writeConfiguredNewProjectExportBufferExtras(
                settings, exportBufferProjects[project], configured);
        }
        settings.sync();
    }

    {
        QSettings settings = settingsStore();
        saveLegacyUvPromotionPolicy(
            settings,
            static_cast<LegacyUvPromotionPolicy>(
                legacyUvPolicy->currentData().toInt()));
        settings.setValue(
            "recovery/autosave_enabled",
            recoveryEnabledCheck->isChecked());
        settings.setValue(
            "recovery/autosave_interval_minutes",
            recoveryIntervalSpin->value());
        settings.sync();
        configureRecoveryTimerFromSettings();
    }

    // Apply and save keymap preferences.
    {
        std::unordered_map<std::string, QList<QKeySequence>> editsByAction{};
        editsByAction.reserve(keymapActionsById.size());
        for ( const KeymapEditRow& row : keymapRows )
        {
            if ( row.controls )
            {
                editsByAction.emplace(row.actionId.toStdString(), row.controls->shortcuts());
            }
        }
        QSettings settings = settingsStore();
        settings.beginGroup("keymap/shortcuts");
        settings.remove("");
        for ( auto& [actionId, action] : keymapActionsById )
        {
            if ( !action )
            {
                continue;
            }
            const QList<QKeySequence> shortcuts = editsByAction[actionId];
            action->setShortcuts(shortcuts);

            QStringList configuredStrings{};
            configuredStrings.reserve(shortcuts.size());
            for (const QKeySequence& seq : shortcuts)
            {
                const QString portable = seq.toString(QKeySequence::PortableText).trimmed();
                if (!portable.isEmpty())
                {
                    configuredStrings.push_back(portable);
                }
            }

            const QStringList defaults = action->property("sed_default_shortcuts").toStringList();
            if (configuredStrings != defaults)
            {
                settings.setValue(QString::fromStdString(actionId), configuredStrings);
            }
        }

        settings.endGroup();
        syncSelectionModifierBindings();
    }

    // Apply and save general resource-preview preferences.
    {
        QSettings settings = settingsStore();

        if (auto* spin = dialog.findChild<QSpinBox*>(
                "pref_resource_preview_worker_count"))
        {
            const int workerCount = spin->value();
            settings.setValue(
                "resource_previews/worker_count", workerCount);
            ui::detail::setPreviewWorkerCount(workerCount);
        }
        if (auto* spin = dialog.findChild<QSpinBox*>(
                "pref_resource_preview_publish_batch_count"))
        {
            const int publishBatchCount = spin->value();
            settings.setValue(
                "resource_previews/publish_batch_count", publishBatchCount);
            ui::detail::setPreviewPublishBatchCount(publishBatchCount);
        }
        settings.sync();
    }

    // Apply file logging immediately so a user can raise the detail level
    // for the next operation without restarting SED.
    {
        QSettings settings = settingsStore();
        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_logging_enabled"))
        {
            settings.setValue("logging/enabled", check->isChecked());
        }
        if (auto* combo = dialog.findChild<QComboBox*>(
                "pref_logging_minimum_level"))
        {
            settings.setValue(
                "logging/minimum_level", combo->currentData().toInt());
        }
        if (auto* spin = dialog.findChild<QSpinBox*>(
                "pref_logging_maximum_file_mib"))
        {
            settings.setValue(
                "logging/maximum_file_mib", spin->value());
        }
        const std::array<std::pair<const char*, const char*>, 4> categories{
            std::pair{ "pref_logging_category_application",
                "logging/category_application" },
            std::pair{ "pref_logging_category_gui",
                "logging/category_gui" },
            std::pair{ "pref_logging_category_core",
                "logging/category_core" },
            std::pair{ "pref_logging_category_resources",
                "logging/category_resources" }
        };
        for (const auto& [objectName, settingName] : categories)
        {
            if (auto* check = dialog.findChild<QCheckBox*>(objectName))
            {
                settings.setValue(settingName, check->isChecked());
            }
        }
        settings.sync();
        reloadApplicationLogSettings();
    }

    // Apply and save viewport preferences.
    {
        QSettings settings = settingsStore();

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_cube_size"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/thing_cube_half_size", static_cast<double>(val));
            if (glView_)
            {
                glView_->setThingCubeHalfSize(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_thing_template_pick_overrides_name"))
        {
            settings.setValue(
                "things/template_pick_overrides_name", check->isChecked());
        }

        const auto applySnapValue = [this, &dialog, &settings](
                                        const char* objectName,
                                        const char* settingKey,
                                        const auto setter)
        {
            if (auto* spin = dialog.findChild<QDoubleSpinBox*>(objectName))
            {
                const float value = static_cast<float>(spin->value());
                settings.setValue(settingKey, static_cast<double>(value));
                if (glView_)
                {
                    (glView_->*setter)(value);
                }
            }
        };
        applySnapValue("pref_snap_radius_vertex",
                       "viewport/snap_radius_vertex",
                       &GlView::setVertexSnapRadius);
        applySnapValue("pref_snap_radius_edge",
                       "viewport/snap_radius_edge",
                       &GlView::setEdgeSnapRadius);
        applySnapValue("pref_snap_radius_face",
                       "viewport/snap_radius_face",
                       &GlView::setFaceSnapRadius);
        applySnapValue("pref_snap_radius_sector",
                       "viewport/snap_radius_sector",
                       &GlView::setSectorSnapRadius);
        applySnapValue("pref_snap_radius_thing",
                       "viewport/snap_radius_thing",
                       &GlView::setThingSnapRadius);
        applySnapValue("pref_snap_rotation_max_degrees",
                       "viewport/snap_rotation_max_degrees",
                       &GlView::setSnapRotationMaxDegrees);
        applySnapValue("pref_surface_stick_radius",
                       "viewport/surface_stick_radius",
                       &GlView::setSurfaceStickRadius);
        applySnapValue("pref_grid_plane_stick_radius",
                       "viewport/grid_plane_stick_radius",
                       &GlView::setGridPlaneStickRadius);

        if (auto* combo = dialog.findChild<QComboBox*>("pref_dynamic_light_shading"))
        {
            const int value = std::clamp(
                combo->currentData().toInt(),
                static_cast<int>(GlView::DynamicLightShading::Gouraud),
                static_cast<int>(GlView::DynamicLightShading::Phong));
            settings.setValue("viewport/dynamic_light_shading", value);
            if (glView_)
            {
                glView_->setDynamicLightShading(
                    static_cast<GlView::DynamicLightShading>(value));
            }
        }

        settings.setValue("viewport/thing_icon_cube_tint", iconCubeTint.name(QColor::HexRgb));
        if (glView_)
        {
            glView_->setThingIconCubeTintColor(iconCubeTint);
        }

        const render::ViewportColorPalette activeViewportPalette =
            captureViewportPalette();
        settings.setValue(
            QString::fromLatin1(render::kViewportColorThemeSetting),
            viewportColorTheme);
        render::saveViewportColorPalette(
            settings, QStringLiteral("viewport/colors"), activeViewportPalette);
        settings.setValue(
            "uv_editor/colors/canvas_background",
            uvCanvasBackgroundColor.name(QColor::HexRgb));
        settings.setValue(
            "uv_editor/colors/grid_main", uvGridMainColor.name(QColor::HexRgb));
        settings.setValue(
            "uv_editor/colors/grid_sub", uvGridSubColor.name(QColor::HexRgb));
        settings.setValue(
            "uv_editor/colors/grid_axis_u", uvGridAxisUColor.name(QColor::HexRgb));
        settings.setValue(
            "uv_editor/colors/grid_axis_v", uvGridAxisVColor.name(QColor::HexRgb));
        if (auto* uvCanvas = findChild<QWidget*>(QStringLiteral("uvEditorCanvas")))
        {
            uvCanvas->setProperty(
                "sed.uvCanvasBackgroundColor", uvCanvasBackgroundColor);
            uvCanvas->setProperty("sed.uvGridMainColor", uvGridMainColor);
            uvCanvas->setProperty("sed.uvGridSubColor", uvGridSubColor);
            uvCanvas->setProperty("sed.uvGridAxisUColor", uvGridAxisUColor);
            uvCanvas->setProperty("sed.uvGridAxisVColor", uvGridAxisVColor);
            uvCanvas->update();
        }
        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_viewport_wire_line_width"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/wireframe_line_width_px", static_cast<double>(val));
            if (glView_)
            {
                glView_->setWireframeLineWidthPx(val);
            }
        }
        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_viewport_grid_line_width"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/grid_line_width_px", static_cast<double>(val));
            if (glView_)
            {
                glView_->setGridLineWidthPx(val);
            }
        }
        if ( auto* combo = dialog.findChild<QComboBox*>(
                "pref_viewport_grid_minor_style") )
        {
            const bool dots = combo->currentData().toBool();
            settings.setValue("viewport/grid_minor_dots", dots);
            if ( glView_ )
            {
                glView_->setGridMinorDots(dots);
            }
        }
        if ( auto* spin = dialog.findChild<QDoubleSpinBox*>(
                "pref_viewport_grid_minor_dot_size") )
        {
            const float sizePx = render::clampedGridMinorDotSizePx(
                static_cast<float>(spin->value()));
            settings.setValue(
                "viewport/grid_minor_dot_size_px",
                static_cast<double>(sizePx));
            if ( glView_ )
            {
                glView_->setGridMinorDotSizePx(sizePx);
            }
        }
        if (auto* check = dialog.findChild<QCheckBox*>("pref_viewport_wire_aa"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/wireframe_antialiasing", val);
            if (glView_)
            {
                glView_->setWireframeAntialiasingEnabled(val);
            }
        }
        if ( auto* combo = dialog.findChild<QComboBox*>("pref_viewport_msaa_samples") )
        {
            settings.setValue("viewport/msaa_samples", combo->currentData().toInt());
        }
        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_viewport_frame_path_line_world_size"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/frame_path_line_world_size", static_cast<double>(val));
            if (glView_)
            {
                glView_->setFramePathLineWorldSize(val);
            }
        }
        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_viewport_frame_path_point_world_size"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/frame_path_point_world_size", static_cast<double>(val));
            if (glView_)
            {
                glView_->setFramePathPointWorldSize(val);
            }
        }
        if (auto* check = dialog.findChild<QCheckBox*>("pref_asset_preview_auto_rotate"))
        {
            settings.setValue("preview/asset_auto_rotate_enabled", check->isChecked());
        }
        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_asset_preview_auto_rotate_speed"))
        {
            settings.setValue("preview/asset_auto_rotate_speed_deg_per_sec", spin->value());
        }
        if (auto* combo = dialog.findChild<QComboBox*>("pref_template_list_model_thumb_mode"))
        {
            settings.setValue("preview/template_list_3do_thumb_mode", combo->currentData().toInt());
        }
        if (auto* check = dialog.findChild<QCheckBox*>("pref_allow_base_templates"))
        {
            settings.setValue("templates/allow_base_template_pick", check->isChecked());
        }
        if (glView_)
        {
            glView_->setBackgroundClearColor(viewportBgClear);
            glView_->setWireframeColor(viewportWireColor);
            glView_->setGridColor(viewportGridColor);
            glView_->setGridMajorColor(viewportGridMajorColor);
            glView_->setGridAxisXColor(viewportGridAxisXColor);
            glView_->setGridAxisYColor(viewportGridAxisYColor);
            glView_->setGridAxisZColor(viewportGridAxisZColor);
            glView_->setSelectionPrimaryColor(viewportSelPrimary);
            glView_->setSelectionMultiColor(viewportSelMulti);
            glView_->setSelectionEdgeMultiColor(viewportSelEdgeMulti);
            glView_->setSelectionActiveColor(viewportSelActive);
            glView_->setSelectionVertexColor(viewportSelVertex);
            glView_->setSelectionVertexMultiColor(viewportSelVertexMulti);
            glView_->setCogEntityPickPrimaryColor(viewportCogPickPrimary);
            glView_->setCogEntityPickActiveColor(viewportCogPickActive);
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_layer_wireframe_colors"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/layers/color_wireframe", val);
            layerViewportPreferencesChanged =
                layerViewportPreferencesChanged
                || wireframeColorByLayer_ != val;
            wireframeColorByLayer_ = val;
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_layer_outliner_colors"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/layers/color_outliner_entities", val);
            layerOutlinerPreferenceChanged =
                layerOutlinerPreferenceChanged
                || outlinerColorByLayer_ != val;
            outlinerColorByLayer_ = val;
        }

        normalizeLayerVisualState();
        QStringList storedLayerColors{};
        storedLayerColors.reserve(static_cast<int>(layerColors_.size()));
        for (const QColor& color : layerColors_)
        {
            storedLayerColors.push_back(color.isValid() ? color.name(QColor::HexRgb) : QString());
        }
        settings.setValue("viewport/layers/colors", storedLayerColors);

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_gizmo_world_size"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/thing_gizmo_world_size", static_cast<double>(val));
            if (glView_)
            {
                glView_->setThingGizmoWorldSize(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_gizmo_model_top_height"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue(
                "viewport/thing_gizmo_height_above_model",
                static_cast<double>(val));
            if (glView_)
            {
                glView_->setThingGizmoHeightAboveModel(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_gizmo_stack_spacing"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue(
                "viewport/thing_gizmo_stack_spacing",
                static_cast<double>(val));
            if (glView_)
            {
                glView_->setThingGizmoStackSpacing(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>(
                "pref_thing_light_gizmo_offset"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue(
                "viewport/thing_light_gizmo_offset_above_model",
                static_cast<double>(val));
            if (glView_)
            {
                glView_->setThingLightGizmoOffsetAboveModel(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>(
                "pref_arch_thing_gizmo_offset"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue(
                "viewport/arch_thing_gizmo_offset_above_model",
                static_cast<double>(val));
            if (glView_)
            {
                glView_->setArchThingGizmoOffsetAboveModel(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>(
                "pref_level_geometry_vertex_marker_size_px"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue(
                "viewport/level_geometry_vertex_marker_size_px",
                static_cast<double>(val));
            if (glView_)
            {
                glView_->setLevelGeometryVertexMarkerSizePx(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_light_gizmo_world_size"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/light_gizmo_world_size", static_cast<double>(val));
            if (glView_)
            {
                glView_->setLightGizmoWorldSize(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_light_gizmo_use_light_color"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/light_gizmo_use_light_color", val);
            if (glView_)
            {
                glView_->setLightGizmoUseLightColor(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>(
                "pref_architectural_lighting_marker_size_px"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue(
                "viewport/architectural_lighting_marker_size_px",
                static_cast<double>(val));
            if (glView_)
            {
                glView_->setArchitecturalLightingMarkerSizePx(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_architectural_lighting_marker_use_authored_color"))
        {
            const bool val = check->isChecked();
            settings.setValue(
                "viewport/architectural_lighting_marker_use_authored_color",
                val);
            if (glView_)
            {
                glView_->setArchitecturalLightingMarkerUseAuthoredColor(val);
            }
        }

        settings.setValue(
            "viewport/architectural_lighting_marker_color",
            architecturalLightingMarkerColor.name(QColor::HexRgb));
        if (glView_)
        {
            glView_->setArchitecturalLightingMarkerColor(
                architecturalLightingMarkerColor);
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_viewport_show_grid"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/show_grid", val);
            if (glView_)
            {
                glView_->setShowGrid(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_viewport_orbit_around_selection"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/orbit_around_selection", val);
            if (glView_)
            {
                glView_->setOrbitAroundSelectionEnabled(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_viewport_grid_follow_camera"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/grid_follow_camera", val);
            if (glView_)
            {
                glView_->setGridFollowCamera(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_viewport_grid_rotate_with_view"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/grid_rotate_with_view", val);
            if (glView_)
            {
                glView_->setGridRotateWithView(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_viewport_grid_keep_view_aligned_visible"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/grid_keep_view_aligned_visible", val);
            if (glView_)
            {
                glView_->setGridKeepViewAlignedVisible(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_viewport_grid_major_spacing"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/grid_spacing", static_cast<double>(val));
            if (glView_)
            {
                glView_->setGridMajorSpacing(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_viewport_grid_minor_spacing"))
        {
            const float val = static_cast<float>(spin->value());
            settings.setValue("viewport/grid_minor_spacing", static_cast<double>(val));
            if (glView_)
            {
                glView_->setGridMinorSpacing(val);
            }
        }

        if (auto* spin = dialog.findChild<QSpinBox*>("pref_viewport_grid_opacity"))
        {
            const float opacity = std::clamp(static_cast<float>(spin->value()) / 100.0f, 0.0f, 1.0f);
            settings.setValue("viewport/grid_opacity", static_cast<double>(opacity));
            if (glView_)
            {
                glView_->setGridOpacity(opacity);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_viewport_grid_adaptive_spacing"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/grid_adaptive_spacing", val);
            if (glView_)
            {
                glView_->setGridAdaptiveSpacing(val);
            }
        }

        if (auto* combo = dialog.findChild<QComboBox*>("pref_viewport_grid_visibility"))
        {
            const int mode = combo->currentData().toInt();
            const bool visibleThroughWalls = mode == 1;
            const bool alwaysBehind = mode == 2;
            settings.setValue("viewport/grid_visible_through_walls", visibleThroughWalls);
            settings.setValue("viewport/grid_always_behind", alwaysBehind);
            if (glView_)
            {
                glView_->setGridVisibleThroughWalls(visibleThroughWalls);
                glView_->setGridAlwaysBehind(alwaysBehind);
            }
            if (viewportGridVisibleThroughWallsAction_)
            {
                viewportGridVisibleThroughWallsAction_->setChecked(visibleThroughWalls);
            }
            if (viewportGridAlwaysBehindAction_)
            {
                viewportGridAlwaysBehindAction_->setChecked(alwaysBehind);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_viewport_grid_reset_on_rotate"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/grid_reset_on_rotate", val);
            if (glView_)
            {
                glView_->setGridResetOnCameraRotate(val);
            }
        }

        if (auto* spin = dialog.findChild<QSpinBox*>("pref_uv_editor_max_zoom"))
        {
            settings.setValue(
                "uv_editor/max_zoom",
                std::clamp(spin->value(), 480, 480'000));
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_viewport_transform_geometry_snap"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/transform_geometry_snap", val);
            if (glView_)
            {
                glView_->setGeometryTransformSnapEnabled(val);
            }
            if (viewportSurfaceGeometrySnapButton_)
            {
                const QSignalBlocker blocker(
                    viewportSurfaceGeometrySnapButton_);
                viewportSurfaceGeometrySnapButton_->setChecked(val);
            }
            if (viewportSurfaceGeometryRotateButton_)
            {
                viewportSurfaceGeometryRotateButton_->setEnabled(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_viewport_transform_geometry_rotate"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/transform_geometry_rotate", val);
            if (glView_)
            {
                glView_->setGeometryTransformRotateToNearbyEnabled(val);
            }
            if (viewportSurfaceGeometryRotateButton_)
            {
                const QSignalBlocker blocker(
                    viewportSurfaceGeometryRotateButton_);
                viewportSurfaceGeometryRotateButton_->setChecked(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_viewport_preserve_connected_surface_uvs"))
        {
            const bool val = check->isChecked();
            settings.setValue(
                "viewport/preserve_connected_surface_uvs", val);
            if (glView_)
            {
                glView_->setPreserveConnectedSurfaceUvsEnabled(val);
            }
            if (viewportPreserveConnectedSurfaceUvsButton_)
            {
                const QSignalBlocker blocker(
                    viewportPreserveConnectedSurfaceUvsButton_);
                viewportPreserveConnectedSurfaceUvsButton_->setChecked(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_viewport_show_selected_surface_normals"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/show_selected_surface_normals", val);
            if (glView_)
            {
                glView_->setShowSelectedSurfaceNormals(val);
            }
            if (surfaceNormalsAction_)
            {
                const QSignalBlocker blocker(surfaceNormalsAction_);
                surfaceNormalsAction_->setChecked(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>(
                "pref_viewport_surface_normal_length"))
        {
            const float val = render::clampedSurfaceNormalLengthScale(
                static_cast<float>(spin->value()));
            settings.setValue(
                "viewport/surface_normal_length_scale",
                static_cast<double>(val));
            if (glView_)
            {
                glView_->setSurfaceNormalLengthScale(val);
            }
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>(
                "pref_viewport_surface_normal_arrowhead_size"))
        {
            const float val = render::clampedSurfaceNormalArrowheadScale(
                static_cast<float>(spin->value()));
            settings.setValue(
                "viewport/surface_normal_arrowhead_scale",
                static_cast<double>(val));
            if (glView_)
            {
                glView_->setSurfaceNormalArrowheadScale(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_viewport_surface_normal_show_arrowhead"))
        {
            const bool val = check->isChecked();
            settings.setValue(
                "viewport/surface_normal_show_arrowhead", val);
            if (glView_)
            {
                glView_->setShowSurfaceNormalArrowhead(val);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_preview_respect_hidden_layers"))
        {
            const bool val = check->isChecked();
            settings.setValue("viewport/preview_respect_hidden_layers", val);
            layerViewportPreferencesChanged =
                layerViewportPreferencesChanged
                || previewRespectHiddenLayers_ != val;
            previewRespectHiddenLayers_ = val;
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>(
                "pref_mots_tree_collision_preview_mesh_radius_factor"))
        {
            const float factor = std::clamp(
                static_cast<float>(spin->value()), 0.01f, 10.0f);
            settings.setValue(
                "viewport/mots_tree_collision_preview_mesh_radius_factor",
                static_cast<double>(factor));
            if (glView_)
            {
                glView_->setMotsTreeCollisionPreviewMeshRadiusFactor(factor);
            }
        }

        if (auto* spin = dialog.findChild<QSpinBox*>(
                "pref_surface_extrude_preview_opacity"))
        {
            const float opacity = std::clamp(
                static_cast<float>(spin->value()) / 100.0f,
                0.05f,
                1.0f);
            settings.setValue(
                "viewport/surface_extrude_preview_opacity",
                static_cast<double>(opacity));
            if (glView_)
            {
                glView_->setSurfaceExtrudePreviewOpacity(opacity);
            }
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_outliner_double_click_frame"))
        {
            settings.setValue("viewport/outliner_double_click_frame", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_outliner_double_click_frame_animate"))
        {
            settings.setValue("viewport/outliner_double_click_frame_animate", check->isChecked());
        }

        if (auto* spin = dialog.findChild<QDoubleSpinBox*>("pref_outliner_double_click_distance_scale"))
        {
            settings.setValue("viewport/outliner_double_click_distance_scale", spin->value());
        }

        // Remove legacy pixel-based settings for thing gizmos.
        settings.remove("viewport/thing_gizmo_base_px");
        settings.remove("viewport/thing_gizmo_min_px");
        settings.remove("viewport/thing_gizmo_max_px");
        settings.remove("viewport/thing_gizmo_transition_ms");
        settings.remove("viewport/light_gizmo_base_px");

        // Per-type colors (used by gizmo icons and non-visual cubes).
        for (const auto& entry : typeColors)
        {
            settings.setValue(
                QString("viewport/thing_type_color/%1").arg(QString::fromStdString(entry.first)),
                entry.second.name(QColor::HexRgb));
        }
        if (glView_)
        {
            glView_->setThingTypeColors(typeColors);
        }

        if (previewCubesCheck)
        {
            const bool show = previewCubesCheck->isChecked();
            settings.setValue("viewport/show_preview_cubes", show);
            if (glView_)
            {
                glView_->setShowPreviewCubes(show);
            }
        }

        if (editorGizmosCheck)
        {
            const bool show = editorGizmosCheck->isChecked();
            settings.setValue("viewport/editor_show_gizmos", show);
            if (glView_)
            {
                glView_->setEditorDefaultShowThingGizmos(show);
            }
        }

        if (previewGizmosCheck)
        {
            const bool show = previewGizmosCheck->isChecked();
            settings.setValue("viewport/preview_show_gizmos", show);
            if (glView_)
            {
                glView_->setPreviewDefaultShowThingGizmos(show);
            }
        }

        if (editorArchGizmosCheck)
        {
            const bool show = editorArchGizmosCheck->isChecked();
            settings.setValue("viewport/arch_thing_gizmos", show);
            if (glView_)
            {
                glView_->setEditorDefaultShowArchThingGizmos(show);
            }
        }

        if (previewArchGizmosCheck)
        {
            const bool show = previewArchGizmosCheck->isChecked();
            settings.setValue("viewport/preview_arch_thing_gizmos", show);
            if (glView_)
            {
                glView_->setPreviewDefaultShowArchThingGizmos(show);
            }
        }

        // Apply overlay visibility immediately if we're currently in preview mode.
        if (layerViewportPreferencesChanged)
        {
            syncLayerViewState();
        }
        if (layerOutlinerPreferenceChanged)
        {
            rebuildOutliner();
        }
        setEditMode(editMode_);
    }

    applyUvEditorMaximumZoomForPreferences();

    // Apply and save COG editor preferences.
    {
        QSettings settings = settingsStore();

        QString selectedThemeId = defaultCogScriptTheme().id;
        if (auto* combo = dialog.findChild<QComboBox*>("pref_cog_editor_theme"))
        {
            selectedThemeId = combo->currentData().toString();
            if (selectedThemeId.trimmed().isEmpty())
            {
                selectedThemeId = defaultCogScriptTheme().id;
            }
            settings.setValue("cog_editor/theme", selectedThemeId);
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_show_whitespace"))
        {
            settings.setValue("cog_editor/show_whitespace", check->isChecked());
        }

        if (auto* combo = dialog.findChild<QComboBox*>(
                "pref_cog_editor_font_family"))
        {
            const QString familyList = combo->currentText().trimmed();
            settings.setValue(
                "cog_editor/font_family",
                familyList.isEmpty()
                    ? ui::cog::defaultCogScriptEditorFontFamilyList()
                    : familyList);
        }

        if (auto* spin = dialog.findChild<QSpinBox*>("pref_cog_editor_font_size"))
        {
            settings.setValue("cog_editor/font_size", spin->value());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_font_bold"))
        {
            settings.setValue("cog_editor/font_bold", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_font_italic"))
        {
            settings.setValue("cog_editor/font_italic", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_tabs"))
        {
            settings.setValue("cog_editor/format_use_tabs", check->isChecked());
        }

        if (auto* spin = dialog.findChild<QSpinBox*>("pref_cog_editor_format_indent"))
        {
            settings.setValue("cog_editor/format_indent_width", spin->value());
        }

        if (auto* combo = dialog.findChild<QComboBox*>("pref_cog_editor_format_braces"))
        {
            settings.setValue("cog_editor/format_brace_style", combo->currentData().toString());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_indent_sections"))
        {
            settings.setValue("cog_editor/format_indent_sections", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_indent_code_bodies"))
        {
            settings.setValue("cog_editor/format_indent_code_bodies", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_keyword_spacing"))
        {
            settings.setValue("cog_editor/format_keyword_spacing", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_inside_parentheses_spacing"))
        {
            settings.setValue("cog_editor/format_inside_parentheses_spacing", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>(
                "pref_cog_editor_format_function_parentheses_spacing"))
        {
            settings.setValue(
                "cog_editor/format_function_parentheses_spacing",
                check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_for_semicolon_spacing"))
        {
            settings.setValue("cog_editor/format_for_semicolon_spacing", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_align_final_return_label"))
        {
            settings.setValue("cog_editor/format_align_final_return_label", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_indent_comments"))
        {
            settings.setValue("cog_editor/format_indent_comments", check->isChecked());
        }

        if (auto* check = dialog.findChild<QCheckBox*>("pref_cog_editor_format_on_save"))
        {
            settings.setValue("cog_editor/format_on_save", check->isChecked());
        }

        const CogScriptTheme selectedTheme = cogScriptThemeById(settings, selectedThemeId);
        for (const CogScriptThemeColorDef& def : cogScriptThemeColorDefs())
        {
            if (auto* button = dialog.findChild<QToolButton*>(QStringLiteral("pref_cog_editor_color_%1").arg(def.key)))
            {
                const QVariant colorVariant = button->property("sed.colorValue");
                const QColor color = colorVariant.value<QColor>();
                const QString settingKey = cogScriptThemeColorSettingKey(def.key);
                if (!color.isValid() || color == selectedTheme.*(def.member))
                {
                    settings.remove(settingKey);
                }
                else
                {
                    settings.setValue(settingKey, color.name(QColor::HexArgb));
                }
            }
        }
    }

    if (cogAppearanceChanged)
    {
        refreshCogScriptEditorAppearance();
    }

    if (activeResourceConfigurationChanged)
    {
        // Mounting/indexing several resource archives is the only Preferences
        // operation that can legitimately take seconds.  Stage it away from
        // the GUI thread and retain the current locator until the complete
        // replacement is ready.  A short operation never flashes feedback;
        // after 300 ms one plain status message reports the still-running
        // work without blocking normal editor input.
        QSettings resourceSettings = settingsStore();
        resourceSettings.sync();

        using ResourceStagePtr =
            std::shared_ptr<StagedProjectResourceState>;
        auto* watcher = new QFutureWatcher<ResourceStagePtr>(this);
        const ProjectType expectedKind = resourceKind_;
        const std::filesystem::path expectedPath = currentPath_;
        const std::uint64_t expectedDocumentEpoch = recoveryDocumentEpoch_;
        const qulonglong generation = property(
            "sed.preferencesResourceUpdateGeneration").toULongLong() + 1u;
        setProperty("sed.preferencesResourceUpdateGeneration", generation);

        QTimer::singleShot(300, watcher,
            [this, watcher, generation]()
            {
                if (watcher->isFinished()
                    || property("sed.preferencesResourceUpdateGeneration")
                        .toULongLong() != generation)
                {
                    return;
                }
                const QString progressMessage =
                    QStringLiteral("Updating project resources...");
                const bool priorProgressVisible = property(
                    "sed.preferencesResourceProgressVisible").toBool();
                if (!priorProgressVisible
                    || statusBar()->currentMessage() != progressMessage)
                {
                    setProperty(
                        "sed.preferencesResourcePriorStatusMessage",
                        statusBar()->currentMessage());
                }
                setProperty(
                    "sed.preferencesResourceProgressVisible", true);
                statusBar()->showMessage(progressMessage);
            });

        QObject::connect(watcher,
            &QFutureWatcher<ResourceStagePtr>::finished, this,
            [this, watcher, generation, expectedKind, expectedPath,
             expectedDocumentEpoch]()
            {
                ResourceStagePtr staged{};
                try
                {
                    staged = watcher->result();
                }
                catch (const std::exception& exception)
                {
                    logMessage(MessageSeverity::warning,
                        QStringLiteral("Could not update project resources: %1")
                            .arg(QString::fromUtf8(exception.what())));
                }
                catch (...)
                {
                    logMessage(MessageSeverity::warning,
                        QStringLiteral("Could not update project resources."));
                }
                watcher->deleteLater();

                const bool currentGeneration = property(
                    "sed.preferencesResourceUpdateGeneration").toULongLong()
                    == generation;
                if (currentGeneration && property(
                        "sed.preferencesResourceProgressVisible").toBool())
                {
                    const QString progressMessage =
                        QStringLiteral("Updating project resources...");
                    const QString priorMessage = property(
                        "sed.preferencesResourcePriorStatusMessage")
                        .toString();
                    if (statusBar()->currentMessage() == progressMessage)
                    {
                        if (priorMessage.isEmpty())
                        {
                            statusBar()->clearMessage();
                        }
                        else
                        {
                            statusBar()->showMessage(priorMessage);
                        }
                    }
                    setProperty(
                        "sed.preferencesResourceProgressVisible", false);
                    setProperty(
                        "sed.preferencesResourcePriorStatusMessage",
                        QVariant{});
                }
                if (!staged || !currentGeneration
                    || recoveryDocumentEpoch_ != expectedDocumentEpoch
                    || resourceKind_ != expectedKind
                    || currentPath_ != expectedPath)
                {
                    return;
                }

                persistStagedProjectResources(*staged);
                reportStagedProjectResources(*staged);
                invalidateThingPlacementSnapCaches();
                resourceKind_ = staged->kind;
                resourceRoots_ = std::move(staged->roots);
                resourceGobs_ = std::move(staged->gobs);
                resources_ = std::move(staged->locator);
                ++legacyUvResourceGeneration_;
                publishResourceLocatorSnapshot(
                    std::move(staged->locatorSnapshot));
                if (glView_)
                {
                    glView_->setResourceLocator(&resources_);
                }
                logMessage(MessageSeverity::info,
                    QStringLiteral("Project resources updated."));
                scheduleLegacyUvPromotionRetry();
            });

        watcher->setFuture(QtConcurrent::run(
            [expectedKind, expectedPath]() -> ResourceStagePtr
            {
                return std::make_shared<StagedProjectResourceState>(
                    stageProjectResources(expectedKind, expectedPath));
            }));
    }
    else
    {
        scheduleLegacyUvPromotionRetry();
    }

    logMessage(MessageSeverity::info, "Preferences saved.");
}

} // namespace sed
