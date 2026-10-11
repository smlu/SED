#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/resources/resource_authoring_detail.hpp"
#include "ui/resources/resource_authoring_pane.hpp"

#include "gl_view.hpp"
#include "sed/assets/bounded_stream_reader.hpp"
#include "sed/assets/episode.hpp"
#include "sed/assets/snd.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/string_table.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/geometry.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"
#include "sed/values.hpp"
#include "ui/episode/episode_editor.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/components/loose_document_watcher.hpp"
#include "ui/components/property_editor_controls.hpp"
#include "ui/components/resource_unsaved_changes_prompt.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/components/viewport_icons.hpp"
#include "ui/resources/resource_picker.hpp"
#include "ui/resources/archive_source_tooltip.hpp"
#include "ui/audio/app_sound_player.hpp"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCborArray>
#include <QCborValue>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QCompleter>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QEnterEvent>
#include <QCursor>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QFileInfo>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QColor>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QShowEvent>
#include <QStyle>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QToolButton>
#include <QUuid>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace sed::ui::resources
{

namespace detail
{


constexpr std::size_t kMaximumResourceAuthoringFileBytes = 16U * 1024U * 1024U;

assets::SprProfile defaultSpriteProfile(const ProjectType projectType) noexcept
{
    return projectType == ProjectType::IJIM
        ? assets::SprProfile::ijim
        : assets::SprProfile::classic;
}

assets::SoundClassProfile soundProfile(const ProjectType projectType) noexcept
{
    return projectType == ProjectType::IJIM
        ? assets::SoundClassProfile::ijim
        : assets::SoundClassProfile::classic;
}

QString qPath(const std::filesystem::path& path)
{
    return path.empty() ? QString{} : QString::fromStdWString(path.wstring());
}

QString normalizedLooseIdentity(const std::filesystem::path& path)
{
    std::error_code error{};
    const std::filesystem::path absolute = std::filesystem::weakly_canonical(path, error);
    const std::filesystem::path normalized = error ? path.lexically_normal() : absolute;
    return QStringLiteral("loose:") + QDir::cleanPath(qPath(normalized)).toCaseFolded();
}

QByteArray sha256(const std::string_view bytes)
{
    return QCryptographicHash::hash(
        QByteArray::fromRawData(bytes.data(), static_cast<qsizetype>(bytes.size())),
        QCryptographicHash::Sha256);
}

std::optional<std::string> readFileBytes(const std::filesystem::path& path)
{
    std::error_code sizeError{};
    const std::uintmax_t byteCount = std::filesystem::file_size(path, sizeError);
    if ( sizeError )
    {
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);
    if ( !input )
    {
        return std::nullopt;
    }
    bool exceededBudget = false;
    const std::size_t sizeHint = byteCount
            > static_cast<std::uintmax_t>(kMaximumResourceAuthoringFileBytes)
        ? kMaximumResourceAuthoringFileBytes
        : static_cast<std::size_t>(byteCount);
    auto bytes = sed::assets::detail::readBinaryStreamWithinBudget<std::string>(
        input,
        kMaximumResourceAuthoringFileBytes,
        sizeHint,
        exceededBudget,
        "Could not read the selected resource.");
    if ( exceededBudget )
    {
        throw std::runtime_error("Resource exceeds byte budget: " + path.string());
    }
    return bytes;
}

std::optional<std::filesystem::path> canonicalPathForWrite(
    const std::filesystem::path& input)
{
    std::error_code error{};
    std::filesystem::path absolute = std::filesystem::absolute(input, error);
    if ( error )
    {
        return std::nullopt;
    }
    absolute = absolute.lexically_normal();
    std::vector<std::filesystem::path> missingTail{};
    std::filesystem::path existing = absolute;
    while ( !existing.empty() && !std::filesystem::exists(existing, error) )
    {
        if ( error )
        {
            return std::nullopt;
        }
        missingTail.push_back(existing.filename());
        existing = existing.parent_path();
    }
    if ( existing.empty() )
    {
        return std::nullopt;
    }
    std::filesystem::path result = std::filesystem::weakly_canonical(existing, error);
    if ( error )
    {
        return std::nullopt;
    }
    for ( auto it = missingTail.rbegin(); it != missingTail.rend(); ++it )
    {
        result /= *it;
    }
    return result.lexically_normal();
}

std::string toUtf8(const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString floatText(const float value)
{
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::general);
    if ( result.ec != std::errc{} )
    {
        return QStringLiteral("0");
    }
    return QString::fromLatin1(buffer.data(), static_cast<qsizetype>(result.ptr - buffer.data()));
}

std::optional<float> parseFiniteFloat(const QString& text)
{
    const QByteArray bytes = text.trimmed().toLatin1();
    if ( bytes.isEmpty() )
    {
        return std::nullopt;
    }
    float value{};
    const auto result = std::from_chars(
        bytes.constData(), bytes.constData() + bytes.size(), value,
        std::chars_format::general);
    if ( result.ec != std::errc{}
        || result.ptr != bytes.constData() + bytes.size()
        || !std::isfinite(value) )
    {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint32_t> parseHexFlags(const QString& text)
{
    QByteArray bytes = text.trimmed().toLatin1();
    if ( bytes.size() >= 2
        && bytes.left(2).compare(QByteArrayLiteral("0x"), Qt::CaseInsensitive) == 0 )
    {
        bytes.remove(0, 2);
    }
    if ( bytes.isEmpty() )
    {
        return std::nullopt;
    }
    std::uint32_t value{};
    const auto result = std::from_chars(
        bytes.constData(), bytes.constData() + bytes.size(), value, 16);
    if ( result.ec != std::errc{} || result.ptr != bytes.constData() + bytes.size() )
    {
        return std::nullopt;
    }
    return value;
}

QString flagsText(const std::uint32_t flags)
{
    constexpr char digits[] = "0123456789ABCDEF";
    QString text = QStringLiteral("0x00000000");
    for ( std::size_t index = 0; index < 8; ++index )
    {
        const unsigned shift = static_cast<unsigned>((7 - index) * 4);
        const std::uint32_t digit = (flags >> shift) & 0x0FU;
        text[static_cast<qsizetype>(index + 2)] = QLatin1Char(digits[digit]);
    }
    return text;
}

namespace
{

QCborMap captureLineEditCaret(const QLineEdit* const edit)
{
    if ( !edit )
    {
        return {};
    }
    const int cursor = edit->cursorPosition();
    int anchor = cursor;
    if ( edit->hasSelectedText() )
    {
        const int start = edit->selectionStart();
        const int end = start + edit->selectedText().size();
        anchor = cursor == start ? end : start;
    }
    return QCborMap{
        {QStringLiteral("cursor"), cursor},
        {QStringLiteral("anchor"), anchor},
    };
}

void restoreLineEditCaret(QLineEdit* const edit, const QCborMap& state)
{
    if ( !edit || !state.value(QStringLiteral("cursor")).isInteger()
        || !state.value(QStringLiteral("anchor")).isInteger() )
    {
        return;
    }
    const QString text = edit->text();
    const int textLength = text.size();
    const int cursor = std::clamp(
        static_cast<int>(state.value(QStringLiteral("cursor")).toInteger()), 0, textLength);
    const int anchor = std::clamp(
        static_cast<int>(state.value(QStringLiteral("anchor")).toInteger()), 0, textLength);
    edit->setSelection(anchor, cursor - anchor);
}

QLineEdit* lineEditForTableEditor(QWidget* const editor)
{
    if ( auto* const direct = qobject_cast<QLineEdit*>(editor) )
    {
        return direct;
    }
    if ( auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(editor) )
    {
        return scrub->lineEdit();
    }
    if ( auto* const combo = qobject_cast<QComboBox*>(editor) )
    {
        return combo->lineEdit();
    }
    return editor ? editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
}

QString tableEditorPresentationKey(
    const QAbstractItemView* const view,
    const QModelIndex& index)
{
    if ( !view || !index.isValid() )
    {
        return {};
    }
    if ( qobject_cast<const QTableWidget*>(view) )
    {
        return QStringLiteral("uni.entries/%1/%2").arg(index.row()).arg(index.column());
    }
    if ( qobject_cast<const QTreeWidget*>(view) )
    {
        const QModelIndex source = index.siblingAtColumn(0);
        const int ordinal = source.data(detail::kSoundTreeSourceIndexRole).toInt();
        if ( ordinal < 0 )
        {
            return {};
        }
        return QStringLiteral("snd.entries/%1/%2").arg(ordinal).arg(index.column());
    }
    return {};
}

} // namespace

ResourceAuthoringDialog::ResourceAuthoringDialog(
    QWidget* parent,
    const ResourceAuthoringKind kind,
    ResourceAuthoringContext context,
    const bool embedded,
    ResourceAuthoringChangedCallback changed)
    : QDialog(parent),
      kind_(kind),
      context_(std::move(context)),
      spriteProfile_(defaultSpriteProfile(context_.projectType)),
      stringProfile_(context_.projectType == ProjectType::IJIM
              ? assets::UniProfile::ijim
              : assets::UniProfile::classic),
      changed_(std::move(changed)),
      previewResources_(context_.resources
              ? std::make_shared<assets::ResourceLocator>(*context_.resources)
              : std::make_shared<assets::ResourceLocator>())
{
    if ( !context_.resources )
    {
        context_.resources = std::make_shared<const assets::ResourceLocator>();
    }

    Qt::WindowFlags flags = windowFlags();
    flags &= ~Qt::WindowMinMaxButtonsHint;
    setWindowFlags(flags);
    setSizeGripEnabled(true);
    embedded_ = embedded;

    previewTimer_ = new QTimer(this);
    previewTimer_->setSingleShot(true);
    previewTimer_->setInterval(90);
    connect(previewTimer_, &QTimer::timeout, this, [this]()
        {
            rebuildSpritePreview();
        });

    buildCommonChrome();
    switch ( kind_ )
    {
        case ResourceAuthoringKind::sprite:
            setWindowTitle(QStringLiteral("Sprite Editor"));
            buildSpriteEditor();
            initializeNewDocument();
            break;
        case ResourceAuthoringKind::soundClass:
            setWindowTitle(QStringLiteral("Sound Class Editor"));
            buildSoundEditor();
            initializeNewDocument();
            break;
        case ResourceAuthoringKind::stringTable:
            setWindowTitle(QStringLiteral("String Table Editor"));
            buildStringEditor();
            initializeNewDocument();
            break;
    }
    if ( embedded_ )
    {
        setWindowFlags(Qt::Widget);
        setModal(false);
        setAttribute(Qt::WA_DeleteOnClose, true);
        draftName_ = suggestedBaseName();
        resetSoundHistory();
        sourceKind_ = SourceKind::draft;
        draftIdentity_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        updateDocumentLabel();
    }
    if ( embedded_ )
    {
        setMinimumSize(kind_ == ResourceAuthoringKind::sprite ? QSize(560, 340) : QSize(420, 280));
        resize(kind_ == ResourceAuthoringKind::sprite ? QSize(820, 540) : QSize(660, 440));
    }
    else
    {
        setMinimumSize(kind_ == ResourceAuthoringKind::sprite ? QSize(1050, 650) : QSize(780, 480));
        resize(kind_ == ResourceAuthoringKind::sprite ? QSize(1220, 780) : QSize(980, 620));
    }
    refreshDocumentWidgets();
    ui::onUiNumericFormatChanged(this, [this]()
        {
            if ( soundEntries_ )
            {
                soundEntries_->viewport()->update();
            }
        });
}

ResourceAuthoringDialog::~ResourceAuthoringDialog()
{
    ui::stopLooseDocumentWatch(this);
    // QDialog hides its focused children after derived members are destroyed.
    // Retire their pane callbacks while the pending maps and model still live.
    refreshing_ = true;
    changed_ = {};
    for ( QObject* const child : findChildren<QObject*>() )
    {
        child->removeEventFilter(this);
        QObject::disconnect(child, nullptr, this, nullptr);
    }
    stopSoundPreview();
    if ( previewTimer_ )
    {
        previewTimer_->stop();
    }
    if ( spriteAnimationTimer_ )
    {
        spriteAnimationTimer_->stop();
    }
    if ( previewView_ )
    {
        // GlView borrows both Level and template storage. Detach before the
        // backing members and the private locator are destroyed.
        previewView_->setLevel(nullptr, GlView::SetLevelMode::keepView, nullptr);
        previewView_->setTemplateDatabase(nullptr);
        previewView_->setResourceLocator(nullptr, false);
        delete previewView_;
        previewView_ = nullptr;
    }
    if ( previewResources_ )
    {
        previewResources_->clearPrimaryResourceOverride();
    }
}

void ResourceAuthoringDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    updateSpriteAnimationTimer();
    if ( deferredPresentationScroll_.pending )
    {
        QTimer::singleShot(0, this, [this]() { applyDeferredPresentationScroll(); });
    }
}

void ResourceAuthoringDialog::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    stopSoundPreview();
    hideSoundHoverControls();
    updateSpriteAnimationTimer();
}

ResourceAuthoringKind ResourceAuthoringDialog::kind() const noexcept
{
    return kind_;
}

bool ResourceAuthoringDialog::isModified() const noexcept
{
    return dirty_;
}

QString ResourceAuthoringDialog::identity() const
{
    if ( sourceKind_ == SourceKind::loose && !currentPath_.empty() )
    {
        return normalizedLooseIdentity(currentPath_);
    }
    if ( sourceKind_ == SourceKind::projectResource )
    {
        return QStringLiteral("project:") + projectResourceName_.trimmed().toCaseFolded();
    }
    return QStringLiteral("draft:") + draftIdentity_;
}

QString ResourceAuthoringDialog::displayTitle() const
{
    const QString name = sourceKind_ == SourceKind::loose
        ? qPath(currentPath_)
        : draftName_;
    return name.isEmpty() ? suggestedBaseName() : name;
}

QString ResourceAuthoringDialog::looseFilePath() const
{
    return sourceKind_ == SourceKind::loose ? qPath(currentPath_) : QString{};
}

bool ResourceAuthoringDialog::takeSuccessfulOpenNotification() noexcept
{
    const bool wasOpened = successfulOpenNotification_;
    successfulOpenNotification_ = false;
    return wasOpened;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void ResourceAuthoringDialog::setAfterReadHookForTest(ResourceAuthoringPaneAfterReadHook hook)
{
    afterReadHook_ = std::move(hook);
}
#endif


void ResourceAuthoringDialog::rebindProjectDirectory(const std::filesystem::path& projectDirectory)
{
    context_.projectDirectory = projectDirectory;
    context_.startingDirectory = projectDirectory;
}

bool ResourceAuthoringDialog::save(bool saveAs)
{
    return saveDocument(saveAs);
}

bool ResourceAuthoringDialog::confirmClose(bool* discardedAny)
{
    return confirmReplaceDocument(discardedAny);
}

void ResourceAuthoringDialog::closeAfterReplacement()
{
    ui::stopLooseDocumentWatch(this);
    setProperty("resource_authoring.force_close", true);
    close();
}

bool ResourceAuthoringDialog::ownsFocus() const
{
    QWidget* focused = QApplication::focusWidget();
    return focused && (focused == this || isAncestorOf(focused));
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
ResourceAuthoringPaneTestCounters ResourceAuthoringDialog::testCounters() const noexcept
{
    return testCounters_;
}

bool ResourceAuthoringDialog::reloadCurrentLooseSourceForTest(QString* error)
{
    return reloadCurrentLooseSource(error);
}
#endif


#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void ResourceAuthoringDialog::resetTestCounters() const noexcept
{
    testCounters_ = {};
}
#endif

QCborMap ResourceAuthoringDialog::captureEditorPresentation() const
{
    QCborMap presentation{{QStringLiteral("version"), 1}};
    QCborMap lineEdits{};
    const auto capture = [&lineEdits](const QString& key, const QLineEdit* const edit)
    {
        if ( edit )
        {
            lineEdits.insert(key, captureLineEditCaret(edit));
        }
    };
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        capture(QStringLiteral("spr.material"), spriteMaterial_);
        capture(QStringLiteral("spr.width"), spriteWidth_ ? spriteWidth_->lineEdit() : nullptr);
        capture(QStringLiteral("spr.height"), spriteHeight_ ? spriteHeight_->lineEdit() : nullptr);
        capture(QStringLiteral("spr.extra_light.scalar"),
            spriteExtraScalar_ ? spriteExtraScalar_->lineEdit() : nullptr);
        capture(QStringLiteral("spr.offset.x"), spriteOffset_[0] ? spriteOffset_[0]->lineEdit() : nullptr);
        capture(QStringLiteral("spr.offset.y"), spriteOffset_[1] ? spriteOffset_[1]->lineEdit() : nullptr);
        capture(QStringLiteral("spr.offset.z"), spriteOffset_[2] ? spriteOffset_[2]->lineEdit() : nullptr);
    }
    else if ( kind_ == ResourceAuthoringKind::soundClass )
    {
        capture(QStringLiteral("snd.search"), soundSearch_);
    }
    else
    {
        capture(QStringLiteral("uni.search"), stringSearch_);
    }
    presentation.insert(QStringLiteral("lineEdits"), lineEdits);

    QCborMap tables{};
    if ( kind_ == ResourceAuthoringKind::stringTable && stringTable_ )
    {
        QCborMap table{};
        const QModelIndex current = stringTable_->currentIndex();
        if ( current.isValid() )
        {
            table.insert(QStringLiteral("current"), QCborMap{
                {QStringLiteral("row"), current.row()},
                {QStringLiteral("column"), current.column()},
            });
        }
        else
        {
            table.insert(QStringLiteral("current"), QCborValue(QCborValue::Null));
        }
        QCborArray selection{};
        for ( const QTableWidgetSelectionRange& range : stringTable_->selectedRanges() )
        {
            selection.append(QCborMap{
                {QStringLiteral("top"), range.topRow()},
                {QStringLiteral("bottom"), range.bottomRow()},
                {QStringLiteral("left"), range.leftColumn()},
                {QStringLiteral("right"), range.rightColumn()},
            });
        }
        table.insert(QStringLiteral("selection"), selection);
        const bool deferredScroll = deferredPresentationScroll_.pending
            && deferredPresentationScroll_.view.data() == stringTable_
            && deferredPresentationScroll_.presentationGeneration == editorPresentationGeneration_
            && deferredPresentationScroll_.interactionGeneration == presentationInteractionGeneration_;
        table.insert(QStringLiteral("horizontalScroll"), deferredScroll
            ? deferredPresentationScroll_.horizontal : stringTable_->horizontalScrollBar()->value());
        table.insert(QStringLiteral("verticalScroll"), deferredScroll
            ? deferredPresentationScroll_.vertical : stringTable_->verticalScrollBar()->value());
        tables.insert(QStringLiteral("uni.entries"), table);
    }
    else if ( kind_ == ResourceAuthoringKind::soundClass && soundEntries_ )
    {
        QCborMap table{};
        QTreeWidgetItem* const current = soundEntries_->currentItem();
        if ( current )
        {
            const auto itemKind = static_cast<SoundTreeItemKind>(
                current->data(0, detail::kSoundTreeItemKindRole).toInt());
            QCborMap identity{{QStringLiteral("kind"), static_cast<int>(itemKind)}};
            if ( itemKind == SoundTreeItemKind::alternative )
            {
                identity.insert(QStringLiteral("sourceOrdinal"),
                    current->data(0, detail::kSoundTreeSourceIndexRole).toInt());
            }
            else
            {
                identity.insert(QStringLiteral("modeBytes"),
                    current->data(0, detail::kSoundTreeModeBytesRole).toByteArray());
            }
            table.insert(QStringLiteral("current"), identity);
            table.insert(QStringLiteral("column"), soundEntries_->currentColumn());
        }
        else
        {
            table.insert(QStringLiteral("current"), QCborValue(QCborValue::Null));
            table.insert(QStringLiteral("column"), 0);
        }
        const bool deferredScroll = deferredPresentationScroll_.pending
            && deferredPresentationScroll_.view.data() == soundEntries_
            && deferredPresentationScroll_.presentationGeneration == editorPresentationGeneration_
            && deferredPresentationScroll_.interactionGeneration == presentationInteractionGeneration_;
        table.insert(QStringLiteral("horizontalScroll"), deferredScroll
            ? deferredPresentationScroll_.horizontal : soundEntries_->horizontalScrollBar()->value());
        table.insert(QStringLiteral("verticalScroll"), deferredScroll
            ? deferredPresentationScroll_.vertical : soundEntries_->verticalScrollBar()->value());
        table.insert(QStringLiteral("manualWidths"), soundManualColumnWidths_);
        if ( soundManualColumnWidths_ )
        {
            QCborArray widths{};
            for ( int column = 0; column < soundEntries_->columnCount(); ++column )
            {
                widths.append(soundEntries_->columnWidth(column));
            }
            table.insert(QStringLiteral("widths"), widths);
        }
        tables.insert(QStringLiteral("snd.entries"), table);
    }
    presentation.insert(QStringLiteral("tables"), tables);

    QCborMap tableEditors = pendingTableEditorCarets_;
    for ( const LiveTableEditor& tracked : liveTableEditors_ )
    {
        if ( !tracked.view || !tracked.editor || !tracked.index.isValid()
            || tracked.view->indexWidget(tracked.index) != tracked.editor.data() )
        {
            continue;
        }
        const QString key = tableEditorPresentationKey(tracked.view.data(), tracked.index);
        if ( key.isEmpty() )
        {
            continue;
        }
        if ( QLineEdit* const edit = lineEditForTableEditor(tracked.editor.data()) )
        {
            tableEditors.insert(key, captureLineEditCaret(edit));
        }
    }
    presentation.insert(QStringLiteral("tableEditors"), tableEditors);
    return presentation;
}

QCborMap ResourceAuthoringDialog::captureState(const bool includeHistory) const
{
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++testCounters_.captureCalls;
#endif
    QCborMap state{};
    state.insert(QStringLiteral("version"), 1);
    state.insert(QStringLiteral("kind"), static_cast<int>(kind_));
    state.insert(QStringLiteral("profile"),
                 kind_ == ResourceAuthoringKind::sprite ? static_cast<int>(defaultSpriteProfile(context_.projectType))
                 : kind_ == ResourceAuthoringKind::stringTable ? static_cast<int>(stringProfile_)
                                                               : static_cast<int>(soundProfile(context_.projectType)));
    state.insert(QStringLiteral("sourceKind"), static_cast<int>(sourceKind_));
    state.insert(QStringLiteral("name"), draftName_);
    state.insert(QStringLiteral("path"), qPath(currentPath_));
    state.insert(QStringLiteral("projectResource"), projectResourceName_);
    if ( !archiveSourceTooltip_.isEmpty() )
    {
        state.insert(QStringLiteral("archiveTooltip"), archiveSourceTooltip_);
    }
    state.insert(QStringLiteral("draftIdentity"), draftIdentity_);
    state.insert(QStringLiteral("modified"), dirty_ || hasPendingDocumentEdit());
    state.insert(QStringLiteral("diskHash"), diskBaselineHash_);
    state.insert(QStringLiteral("paneGeometry"), saveGeometry());
    state.insert(QStringLiteral("search"), stringSearch_ ? stringSearch_->text() : QString{});
    state.insert(QStringLiteral("soundSearch"), soundSearch_ ? soundSearch_->text() : QString{});
    state.insert(QStringLiteral("editorPresentation"), captureEditorPresentation());
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        state.insert(QStringLiteral("presentation"), captureSpritePresentation());
        if ( !spritePendingNumericEdits_.isEmpty() )
        {
            state.insert(QStringLiteral("spritePendingNumericEdits"), spritePendingNumericEdits_);
        }
    }

    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        QCborArray values{};
        values.append(spriteMaterial_ ? spriteMaterial_->text() : QString{});
        values.append(spriteTypeCombo_ ? spriteTypeCombo_->currentData().toInt() : 0);
        values.append(floatText(sprite_.width));
        values.append(floatText(sprite_.height));
        values.append(spriteGeometryMode_ ? spriteGeometryMode_->currentData().toInt() : 0);
        values.append(spriteLightMode_ ? spriteLightMode_->currentData().toInt() : 0);
        values.append(spriteTextureMode_ ? spriteTextureMode_->currentData().toInt() : 0);
        values.append(floatText(sprite_.extraLightRGBA[0]));
        for ( const float value : sprite_.extraLightRGBA )
        {
            values.append(floatText(value));
        }
        for ( const float value : sprite_.offsetXYZ )
        {
            values.append(floatText(value));
        }
        state.insert(QStringLiteral("sprite"), values);
    }
    else
    {
        QCborArray rows{};
        if ( kind_ == ResourceAuthoringKind::soundClass && soundEntries_ )
        {
            std::vector<std::pair<int, QCborArray>> soundRows{};
            for ( int groupIndex = 0; groupIndex < soundEntries_->topLevelItemCount(); ++groupIndex )
            {
                const QTreeWidgetItem* const group = soundEntries_->topLevelItem(groupIndex);
                for ( int childIndex = 0; group && childIndex < group->childCount(); ++childIndex )
                {
                    const QTreeWidgetItem* const item = group->child(childIndex);
                    if ( !item || item->data(0, kSoundTreeItemKindRole).toInt() !=
                                      static_cast<int>(detail::SoundTreeItemKind::alternative) )
                    {
                        continue;
                    }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    ++testCounters_.rowVisits;
#endif
                    QCborArray cells{};
                    const QByteArray mode = item->data(0, detail::kSoundTreeModeBytesRole).toByteArray();
                    cells.append(ui::episode::displayTextForGameBytes(
                        std::string_view(mode.constData(), static_cast<std::size_t>(mode.size()))));
                    for ( int column = 1; column <= 5; ++column )
                    {
                        const QModelIndex index = soundEntries_->indexFromItem(item, column);
                        cells.append(cellTextWithPendingEditor(soundEntries_, index, item->text(column)));
                    }
                    soundRows.emplace_back(item->data(0, detail::kSoundTreeSourceIndexRole).toInt(), std::move(cells));
                }
            }
            std::sort(soundRows.begin(), soundRows.end(),
                      [](const auto& left, const auto& right)
                      {
                          return left.first < right.first;
                      });
            for ( auto& [sourceIndex, cells] : soundRows )
            {
                (void)sourceIndex;
                rows.append(std::move(cells));
            }
        }
        else if ( stringTable_ )
        {
            for ( int row = 0; row < stringTable_->rowCount(); ++row )
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                ++testCounters_.rowVisits;
#endif
                QCborArray cells{};
                for ( int column = 0; column < stringTable_->columnCount(); ++column )
                {
                    const QTableWidgetItem* const item = stringTable_->item(row, column);
                    cells.append(cellTextWithPendingEditor(stringTable_, stringTable_->model()->index(row, column),
                                                           item ? item->text() : QString{}));
                }
                rows.append(cells);
            }
        }
        state.insert(QStringLiteral("rows"), rows);
        if ( kind_ == ResourceAuthoringKind::stringTable )
        {
            state.insert(QStringLiteral("capacity"), stringsFile_.capacity);
        }
        else
        {
            QCborArray loops{};
            for ( std::size_t row = 0; row < soundFile_.entries.size(); ++row )
            {
                loops.append(row < soundPreviewLoopRows_.size() && soundPreviewLoopRows_[row]);
            }
            state.insert(QStringLiteral("soundPreviewLoops"), loops);
            QSet<QString> representedModes;
            for ( const QCborValue& row : rows )
            {
                representedModes.insert(row.toArray().at(0).toString());
            }
            QStringList emptyModes;
            for ( const QString& mode : soundPresentationModes_ )
            {
                if ( !representedModes.contains(mode) )
                {
                    emptyModes.append(mode);
                }
            }
            emptyModes.sort(Qt::CaseSensitive);
            QCborArray empty;
            for ( const QString& mode : emptyModes )
            {
                empty.append(mode);
            }
            state.insert(QStringLiteral("soundEmptyModes"), empty);
            QStringList collapsedModes;
            for ( int index = 0; soundEntries_ && index < soundEntries_->topLevelItemCount(); ++index )
            {
                const QTreeWidgetItem* const group = soundEntries_->topLevelItem(index);
                if ( group && group->data(0, detail::kSoundTreeItemKindRole).toInt() ==
                                  static_cast<int>(detail::SoundTreeItemKind::mode) )
                {
                    const QString mode = ui::episode::displayTextForGameBytes(
                        group->data(0, detail::kSoundTreeModeBytesRole).toByteArray().toStdString());
                    if ( collapsedSoundModes_.contains(mode) )
                    {
                        collapsedModes.append(mode);
                    }
                }
            }
            collapsedModes.sort(Qt::CaseSensitive);
            QCborArray collapsed;
            for ( const QString& mode : collapsedModes )
            {
                collapsed.append(mode);
            }
            state.insert(QStringLiteral("collapsedModes"), collapsed);
        }
    }
    if ( includeHistory )
    {
        state.insert(QStringLiteral("history"), captureDocumentHistory(state));
    }
    return state;
}

QCborMap ResourceAuthoringDialog::authoredContentState() const
{
    QCborMap state = captureState(false);
    state.remove(QStringLiteral("paneGeometry"));
    state.remove(QStringLiteral("search"));
    state.remove(QStringLiteral("soundSearch"));
    state.remove(QStringLiteral("collapsedModes"));
    state.remove(QStringLiteral("presentation"));
    state.remove(QStringLiteral("editorPresentation"));
    state.remove(QStringLiteral("history"));
    state.remove(QStringLiteral("soundPreviewLoops"));
    state.remove(QStringLiteral("soundEmptyModes"));
    state.remove(QStringLiteral("archiveTooltip"));
    return state;
}

bool ResourceAuthoringDialog::authoringSnapshotStillCurrent(const std::uint64_t revision, const QCborMap& snapshot)
{
    QPointer<ResourceAuthoringDialog> self(this);
    if ( authoredRevision_ != revision || authoredContentState() != snapshot )
    {
        return false;
    }
    const auto contextCheck = context_.projectContextStillCurrent;
    if ( !contextCheck )
    {
        return true;
    }
    const bool contextIsCurrent = contextCheck();
    return self && contextIsCurrent;
}

bool ResourceAuthoringDialog::restoreState(const QCborMap& state, QString* warning)
{
    if ( !validateState(state, warning) )
    {
        return false;
    }
    const int storedProfile = static_cast<int>(state.value(QStringLiteral("profile")).toInteger());
    if ( kind_ == ResourceAuthoringKind::stringTable &&
         storedProfile != static_cast<int>(context_.projectType == ProjectType::IJIM ? assets::UniProfile::ijim
                                                                                     : assets::UniProfile::classic) )
    {
        if ( warning )
        {
            *warning = QStringLiteral("String-table profile does not match the active project.");
        }
        return false;
    }
    cancelDeferredEditorPresentation();
    soundManualColumnWidths_ = false;
    const bool previousRefreshing = refreshing_;
    refreshing_ = true;
    {
        const QSignalBlocker soundSearchBlocker(soundSearch_);
        const QSignalBlocker stringSearchBlocker(stringSearch_);
        sourceKind_ = static_cast<SourceKind>(state.value(QStringLiteral("sourceKind")).toInteger());
        draftName_ = state.value(QStringLiteral("name")).toString();
        currentPath_ = state.value(QStringLiteral("path")).toString().toStdWString();
        projectResourceName_ = state.value(QStringLiteral("projectResource")).toString();
        archiveSourceTooltip_ = state.value(QStringLiteral("archiveTooltip")).toString();
        draftIdentity_ = state.value(QStringLiteral("draftIdentity")).toString();
        dirty_ = state.value(QStringLiteral("modified")).toBool();
        diskBaselineHash_ = state.value(QStringLiteral("diskHash")).toByteArray();
        (void)restoreGeometry(state.value(QStringLiteral("paneGeometry")).toByteArray());
        if ( stringSearch_ )
        {
            stringSearch_->setText(state.value(QStringLiteral("search")).toString());
        }
        if ( soundSearch_ )
        {
            soundSearch_->setText(state.value(QStringLiteral("soundSearch")).toString());
        }
        const int profile = static_cast<int>(state.value(QStringLiteral("profile")).toInteger());
        if ( kind_ == ResourceAuthoringKind::sprite )
        {
            spriteProfile_ = defaultSpriteProfile(context_.projectType);
            const QCborArray values = state.value(QStringLiteral("sprite")).toArray();
            const bool wasRefreshing = refreshing_;
            refreshing_ = true;
            spriteMaterial_->setText(values.at(0).toString());
            selectIntegerCombo(spriteTypeCombo_, static_cast<int>(values.at(1).toInteger()));
            sprite_.width = *parseFiniteFloat(values.at(2).toString());
            sprite_.height = *parseFiniteFloat(values.at(3).toString());
            spriteWidth_->setValue(sprite_.width);
            spriteHeight_->setValue(sprite_.height);
            selectIntegerCombo(spriteGeometryMode_, static_cast<int>(values.at(4).toInteger()));
            selectIntegerCombo(spriteLightMode_, static_cast<int>(values.at(5).toInteger()));
            selectIntegerCombo(spriteTextureMode_, static_cast<int>(values.at(6).toInteger()));
            sprite_.extraLightRGBA[0] = *parseFiniteFloat(values.at(7).toString());
            spriteExtraScalar_->setValue(sprite_.extraLightRGBA[0]);
            for ( std::size_t index = 0; index < sprite_.extraLightRGBA.size(); ++index )
            {
                sprite_.extraLightRGBA[index] =
                    *parseFiniteFloat(values.at(static_cast<qsizetype>(8 + index)).toString());
            }
            for ( std::size_t index = 0; index < spriteOffset_.size(); ++index )
            {
                sprite_.offsetXYZ[index] = *parseFiniteFloat(values.at(static_cast<qsizetype>(12 + index)).toString());
                spriteOffset_[index]->setValue(sprite_.offsetXYZ[index]);
            }
            spritePendingNumericEdits_.clear();
            spriteNumericEditBaselines_.clear();
            spriteNumericUndoKeys_.clear();
            const QCborMap pendingEdits = state.value(QStringLiteral("spritePendingNumericEdits")).toMap();
            for ( const QString& key :
                  {QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("extraLightScalar"),
                   QStringLiteral("offsetX"), QStringLiteral("offsetY"), QStringLiteral("offsetZ")} )
            {
                if ( pendingEdits.contains(key) )
                {
                    spritePendingNumericEdits_.insert(key, pendingEdits.value(key));
                }
            }
            restorePendingSpriteNumericEditors();
            refreshing_ = wasRefreshing;
            if ( const auto material = ui::episode::gameBytesForDisplayText(spriteMaterial_->text()) )
            {
                sprite_.material = *material;
            }
            sprite_.type = spriteTypeCombo_->currentData().toInt();
            sprite_.geometryMode = spriteGeometryMode_->currentData().toInt();
            sprite_.lightMode = spriteLightMode_->currentData().toInt();
            sprite_.textureMode = spriteTextureMode_->currentData().toInt();
            updateSpriteProfileRows();
            scheduleSpritePreview(true);
            if ( state.contains(QStringLiteral("presentation")) )
            {
                restoreSpritePresentation(state.value(QStringLiteral("presentation")).toMap());
            }
        }
        else
        {
            const QCborArray rows = state.value(QStringLiteral("rows")).toArray();
            if ( kind_ == ResourceAuthoringKind::soundClass )
            {
                soundFile_.entries.clear();
                soundRawRows_.clear();
                soundPreviewLoopRows_.clear();
                knownSoundModes_.clear();
                soundPresentationModes_.clear();
                collapsedSoundModes_.clear();
                soundFile_.entries.reserve(static_cast<std::size_t>(rows.size()));
                for ( const QCborValue& rowValue : rows )
                {
                    const QCborArray cells = rowValue.toArray();
                    assets::SoundClassEntry entry{};
                    if ( const auto mode = ui::episode::gameBytesForDisplayText(cells.at(0).toString()) )
                    {
                        entry.mode = *mode;
                    }
                    if ( const auto wav = ui::episode::gameBytesForDisplayText(cells.at(1).toString()) )
                    {
                        entry.wav = *wav;
                    }
                    if ( const auto flags = parseHexFlags(cells.at(2).toString()) )
                    {
                        entry.playFlags = *flags;
                    }
                    if ( const auto value = parseFiniteFloat(cells.at(3).toString()) )
                    {
                        entry.minRadius = *value;
                    }
                    if ( const auto value = parseFiniteFloat(cells.at(4).toString()) )
                    {
                        entry.maxRadius = *value;
                    }
                    if ( const auto value = parseFiniteFloat(cells.at(5).toString()) )
                    {
                        entry.maxVolume = *value;
                    }
                    soundFile_.entries.push_back(std::move(entry));
                }
                for ( const QCborValue& mode : state.value(QStringLiteral("soundEmptyModes")).toArray() )
                {
                    soundPresentationModes_.insert(mode.toString());
                }
                refreshSoundRows();
                restoreSoundRawRows(rows);
                const QCborValue savedLoops = state.value(QStringLiteral("soundPreviewLoops"));
                if ( savedLoops.isArray() )
                {
                    for ( qsizetype row = 0; row < savedLoops.toArray().size(); ++row )
                    {
                        soundPreviewLoopRows_[static_cast<std::size_t>(row)] = savedLoops.toArray().at(row).toBool();
                    }
                }
                syncSoundModelFromRawRows();
                collapsedSoundModes_.clear();
                const QCborArray savedCollapsed = state.value(QStringLiteral("collapsedModes")).toArray();
                for ( int index = 0; index < soundEntries_->topLevelItemCount(); ++index )
                {
                    const QTreeWidgetItem* const group = soundEntries_->topLevelItem(index);
                    if ( !group ||
                         group->data(0, kSoundTreeItemKindRole).toInt() != static_cast<int>(SoundTreeItemKind::mode) )
                    {
                        continue;
                    }
                    const QString identity = ui::episode::displayTextForGameBytes(
                        group->data(0, kSoundTreeModeBytesRole).toByteArray().toStdString());
                    for ( const QCborValue& mode : savedCollapsed )
                    {
                        if ( mode.toString() == identity || mode.toString() == group->text(0) )
                        {
                            collapsedSoundModes_.insert(identity);
                        }
                    }
                }
                refreshSoundModeGroups();
                resetSoundHistory();
            }
            else
            {
                stringProfile_ = static_cast<assets::UniProfile>(profile);
                stringsFile_.capacity = static_cast<std::uint32_t>(state.value(QStringLiteral("capacity")).toInteger());
                stringsFile_.entries.resize(static_cast<std::size_t>(rows.size()));
                refreshStringRows();
                restoreRawRows(stringTable_, rows);
                syncStringModelFromRawRows();
                filterStringRows();
            }
        }
    }
    restoreDocumentHistory(state);
    const QCborValue editorPresentation = state.value(QStringLiteral("editorPresentation"));
    if ( editorPresentation.isMap() )
    {
        restoreEditorPresentation(editorPresentation.toMap());
    }
    refreshing_ = previousRefreshing;
    ++authoredRevision_;
    updateDocumentLabel();
    notifyChanged();
    return true;
}

bool ResourceAuthoringDialog::validateEditorPresentation(
    const QCborMap& presentation,
    const QCborMap& state,
    QString* error)
{
    const auto fail = [error](const QString& message)
    {
        if ( error )
        {
            *error = QStringLiteral("Resource editor presentation: ") + message;
        }
        return false;
    };
    const QCborValue version = presentation.value(QStringLiteral("version"));
    if ( !version.isInteger() || version.toInteger() != 1
        || QCborValue(presentation).toCbor().size() > 1024 * 1024 )
    {
        return fail(QStringLiteral("the version or encoded size is invalid."));
    }

    const int kind = static_cast<int>(state.value(QStringLiteral("kind")).toInteger());
    const QCborArray rows = state.value(QStringLiteral("rows")).toArray();
    const QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
    const QCborMap pendingSprite = state.value(QStringLiteral("spritePendingNumericEdits")).toMap();
    const auto textLengthForLineEdit = [&](const QString& key) -> int
    {
        if ( key == QStringLiteral("spr.material") && sprite.size() > 0 )
        {
            return sprite.at(0).toString().size();
        }
        if ( key == QStringLiteral("spr.width") )
        {
            return pendingSprite.contains(QStringLiteral("width"))
                ? pendingSprite.value(QStringLiteral("width")).toString().size()
                : sprite.size() > 2 ? -2 : -1;
        }
        if ( key == QStringLiteral("spr.height") )
        {
            return pendingSprite.contains(QStringLiteral("height"))
                ? pendingSprite.value(QStringLiteral("height")).toString().size()
                : sprite.size() > 3 ? -2 : -1;
        }
        if ( key == QStringLiteral("spr.extra_light.scalar") )
        {
            return pendingSprite.contains(QStringLiteral("extraLightScalar"))
                ? pendingSprite.value(QStringLiteral("extraLightScalar")).toString().size()
                : sprite.size() > 7 ? -2 : -1;
        }
        if ( key == QStringLiteral("spr.offset.x") || key == QStringLiteral("spr.offset.y")
            || key == QStringLiteral("spr.offset.z") )
        {
            const QString pendingKey = key == QStringLiteral("spr.offset.x") ? QStringLiteral("offsetX")
                : key == QStringLiteral("spr.offset.y") ? QStringLiteral("offsetY")
                                                          : QStringLiteral("offsetZ");
            const int valueIndex = pendingKey == QStringLiteral("offsetX") ? 12
                : pendingKey == QStringLiteral("offsetY") ? 13 : 14;
            return pendingSprite.contains(pendingKey)
                ? pendingSprite.value(pendingKey).toString().size()
                : sprite.size() > valueIndex ? -2 : -1;
        }
        if ( key == QStringLiteral("snd.search") )
        {
            return state.value(QStringLiteral("soundSearch")).toString().size();
        }
        if ( key == QStringLiteral("uni.search") )
        {
            return state.value(QStringLiteral("search")).toString().size();
        }
        return -1;
    };
    const auto validateCaret = [&fail](const QCborMap& caret, const int textLength, const QString& label)
    {
        if ( textLength == -1 || !caret.value(QStringLiteral("cursor")).isInteger()
            || !caret.value(QStringLiteral("anchor")).isInteger() )
        {
            return fail(QStringLiteral("the %1 caret is incomplete or references an unknown editor.").arg(label));
        }
        const qint64 maximum = textLength == -2 ? 65536 : textLength;
        for ( const QString& key : {QStringLiteral("cursor"), QStringLiteral("anchor")} )
        {
            const QCborValue position = caret.value(key);
            if ( position.toInteger() < 0 || position.toInteger() > maximum )
            {
                return fail(QStringLiteral("the %1 %2 position is invalid.").arg(label, key));
            }
        }
        return true;
    };

    const QCborValue lineEditsValue = presentation.value(QStringLiteral("lineEdits"));
    if ( !lineEditsValue.isUndefined() )
    {
        if ( !lineEditsValue.isMap() || lineEditsValue.toMap().size() > 64 )
        {
            return fail(QStringLiteral("the permanent editor map is invalid."));
        }
        const QCborMap lineEdits = lineEditsValue.toMap();
        for ( auto iterator = lineEdits.cbegin(); iterator != lineEdits.cend(); ++iterator )
        {
            if ( !iterator.key().isString() )
            {
                continue;
            }
            const QString key = iterator.key().toString();
            const int textLength = textLengthForLineEdit(key);
            if ( textLength == -1 )
            {
                continue;
            }
            if ( !iterator.value().isMap() )
            {
                return fail(QStringLiteral("the %1 caret entry is not a map.").arg(key));
            }
            if ( !validateCaret(iterator.value().toMap(), textLength, key) )
            {
                return false;
            }
        }
    }

    const QCborValue tablesValue = presentation.value(QStringLiteral("tables"));
    if ( !tablesValue.isUndefined() )
    {
        if ( !tablesValue.isMap() || tablesValue.toMap().size() > 16 )
        {
            return fail(QStringLiteral("the table presentation map is invalid."));
        }
        const QCborMap tables = tablesValue.toMap();
        const QCborValue uniValue = tables.value(QStringLiteral("uni.entries"));
        if ( !uniValue.isUndefined() )
        {
            if ( kind != static_cast<int>(ResourceAuthoringKind::stringTable) || !uniValue.isMap() )
            {
                return fail(QStringLiteral("the UNI table state has the wrong type or editor kind."));
            }
            const QCborMap table = uniValue.toMap();
            const QCborValue current = table.value(QStringLiteral("current"));
            if ( !current.isUndefined() && !current.isNull() )
            {
                if ( !current.isMap() )
                {
                    return fail(QStringLiteral("the UNI current-cell identity is invalid."));
                }
                const QCborMap identity = current.toMap();
                const QCborValue row = identity.value(QStringLiteral("row"));
                const QCborValue column = identity.value(QStringLiteral("column"));
                if ( !row.isInteger() || !column.isInteger() || row.toInteger() < 0
                    || row.toInteger() >= rows.size() || column.toInteger() < 0 || column.toInteger() >= 3 )
                {
                    return fail(QStringLiteral("the UNI current-cell reference is outside the source table."));
                }
            }
            const QCborValue selection = table.value(QStringLiteral("selection"));
            if ( !selection.isUndefined() )
            {
                if ( !selection.isArray() || selection.toArray().size() > 4096 )
                {
                    return fail(QStringLiteral("the UNI selection ranges are invalid."));
                }
                for ( const QCborValue& rangeValue : selection.toArray() )
                {
                    if ( !rangeValue.isMap() )
                    {
                        return fail(QStringLiteral("a UNI selection range is not a map."));
                    }
                    const QCborMap range = rangeValue.toMap();
                    const QCborValue top = range.value(QStringLiteral("top"));
                    const QCborValue bottom = range.value(QStringLiteral("bottom"));
                    const QCborValue left = range.value(QStringLiteral("left"));
                    const QCborValue right = range.value(QStringLiteral("right"));
                    if ( !top.isInteger() || !bottom.isInteger() || !left.isInteger() || !right.isInteger()
                        || top.toInteger() < 0 || bottom.toInteger() < top.toInteger()
                        || bottom.toInteger() >= rows.size() || left.toInteger() < 0 || right.toInteger() < left.toInteger()
                        || right.toInteger() >= 3 )
                    {
                        return fail(QStringLiteral("a UNI selection range is outside the source table."));
                    }
                }
            }
            for ( const QString& key : {QStringLiteral("horizontalScroll"), QStringLiteral("verticalScroll")} )
            {
                const QCborValue offset = table.value(key);
                if ( !offset.isUndefined()
                    && (!offset.isInteger() || offset.toInteger() < 0 || offset.toInteger() > 10000000) )
                {
                    return fail(QStringLiteral("a UNI scroll offset is invalid."));
                }
            }
        }

        const QCborValue sndValue = tables.value(QStringLiteral("snd.entries"));
        if ( !sndValue.isUndefined() )
        {
            if ( kind != static_cast<int>(ResourceAuthoringKind::soundClass) || !sndValue.isMap() )
            {
                return fail(QStringLiteral("the SND table state has the wrong type or editor kind."));
            }
            const QCborMap table = sndValue.toMap();
            const QCborValue current = table.value(QStringLiteral("current"));
            if ( !current.isUndefined() && !current.isNull() )
            {
                if ( !current.isMap() )
                {
                    return fail(QStringLiteral("the SND current-item identity is invalid."));
                }
                const QCborMap identity = current.toMap();
                const QCborValue itemKind = identity.value(QStringLiteral("kind"));
                if ( !itemKind.isInteger() || itemKind.toInteger() < 0
                    || itemKind.toInteger() > static_cast<int>(SoundTreeItemKind::columnHeader) )
                {
                    return fail(QStringLiteral("the SND item kind is invalid."));
                }
                if ( itemKind.toInteger() == static_cast<int>(SoundTreeItemKind::alternative) )
                {
                    const QCborValue ordinal = identity.value(QStringLiteral("sourceOrdinal"));
                    if ( !ordinal.isInteger() || ordinal.toInteger() < 0 || ordinal.toInteger() >= rows.size() )
                    {
                        return fail(QStringLiteral("the SND source ordinal is outside the source rows."));
                    }
                }
                else if ( itemKind.toInteger() == static_cast<int>(SoundTreeItemKind::mode)
                    || itemKind.toInteger() == static_cast<int>(SoundTreeItemKind::addAlternative)
                    || itemKind.toInteger() == static_cast<int>(SoundTreeItemKind::columnHeader) )
                {
                    const QCborValue modeValue = identity.value(QStringLiteral("modeBytes"));
                    if ( !modeValue.isByteArray() || modeValue.toByteArray().size() > 4096 )
                    {
                        return fail(QStringLiteral("the SND mode identity is absent or invalid."));
                    }
                    const QByteArray modeBytes = modeValue.toByteArray();
                    bool matches = false;
                    for ( const QCborValue& rowValue : rows )
                    {
                        const auto rowMode = ui::episode::gameBytesForDisplayText(rowValue.toArray().at(0).toString());
                        if ( rowMode && QByteArray(rowMode->data(), static_cast<qsizetype>(rowMode->size())) == modeBytes )
                        {
                            matches = true;
                            break;
                        }
                    }
                    for ( const QCborValue& emptyMode : state.value(QStringLiteral("soundEmptyModes")).toArray() )
                    {
                        const auto rowMode = ui::episode::gameBytesForDisplayText(emptyMode.toString());
                        if ( rowMode && QByteArray(rowMode->data(), static_cast<qsizetype>(rowMode->size())) == modeBytes )
                        {
                            matches = true;
                            break;
                        }
                    }
                    if ( !matches )
                    {
                        return fail(QStringLiteral("the SND mode reference is absent from the source document."));
                    }
                }
            }
            const QCborValue column = table.value(QStringLiteral("column"));
            if ( !column.isUndefined()
                && (!column.isInteger() || column.toInteger() < 0 || column.toInteger() >= 6) )
            {
                return fail(QStringLiteral("the SND current column is invalid."));
            }
            if ( current.isMap() && !column.isInteger() )
            {
                return fail(QStringLiteral("the SND current item has no valid column."));
            }
            for ( const QString& key : {QStringLiteral("horizontalScroll"), QStringLiteral("verticalScroll")} )
            {
                const QCborValue offset = table.value(key);
                if ( !offset.isUndefined()
                    && (!offset.isInteger() || offset.toInteger() < 0 || offset.toInteger() > 10000000) )
                {
                    return fail(QStringLiteral("a SND scroll offset is invalid."));
                }
            }
            const QCborValue manualWidths = table.value(QStringLiteral("manualWidths"));
            if ( !manualWidths.isUndefined() && !manualWidths.isBool() )
            {
                return fail(QStringLiteral("the SND manual-width flag is invalid."));
            }
            const QCborValue widths = table.value(QStringLiteral("widths"));
            if ( (manualWidths.isBool() && manualWidths.toBool() && !widths.isArray())
                || (!widths.isUndefined() && (!manualWidths.isBool() || !manualWidths.toBool())) )
            {
                return fail(QStringLiteral("the SND manual widths do not match their explicit flag."));
            }
            if ( !widths.isUndefined() )
            {
                static constexpr std::array<int, 6> minima{80, 160, 100, 56, 56, 230};
                if ( !widths.isArray() || widths.toArray().size() != static_cast<qsizetype>(minima.size()) )
                {
                    return fail(QStringLiteral("the SND manual widths do not contain six columns."));
                }
                qint64 total = 0;
                for ( std::size_t index = 0; index < minima.size(); ++index )
                {
                    const QCborValue width = widths.toArray().at(static_cast<qsizetype>(index));
                    if ( !width.isInteger() || width.toInteger() < minima[index] || width.toInteger() > 16384 )
                    {
                        return fail(QStringLiteral("a SND manual width is below its minimum or exceeds its bound."));
                    }
                    total += width.toInteger();
                }
                if ( total > 65536LL )
                {
                    return fail(QStringLiteral("the SND manual width total exceeds its bound."));
                }
            }
        }
    }

    const QCborValue tableEditorsValue = presentation.value(QStringLiteral("tableEditors"));
    if ( !tableEditorsValue.isUndefined() )
    {
        if ( !tableEditorsValue.isMap() || tableEditorsValue.toMap().size() > 4096 )
        {
            return fail(QStringLiteral("the live table-editor caret map is invalid."));
        }
        const QCborMap tableEditors = tableEditorsValue.toMap();
        for ( auto iterator = tableEditors.cbegin(); iterator != tableEditors.cend(); ++iterator )
        {
            if ( !iterator.key().isString() )
            {
                continue;
            }
            const QString key = iterator.key().toString();
            const QStringList parts = key.split(QLatin1Char('/'));
            if ( parts.size() != 3 || (parts.at(0) != QStringLiteral("uni.entries")
                    && parts.at(0) != QStringLiteral("snd.entries")) )
            {
                continue;
            }
            bool rowOk = false;
            bool columnOk = false;
            const int row = parts.at(1).toInt(&rowOk);
            const int column = parts.at(2).toInt(&columnOk);
            const bool sound = parts.at(0) == QStringLiteral("snd.entries");
            const int maxColumn = sound ? 5 : 2;
            if ( !rowOk || !columnOk || row < 0 || row >= rows.size()
                || column < 0 || column > maxColumn
                || (sound && kind != static_cast<int>(ResourceAuthoringKind::soundClass))
                || (!sound && kind != static_cast<int>(ResourceAuthoringKind::stringTable)) )
            {
                return fail(QStringLiteral("a live table-editor caret references an invalid source cell."));
            }
            if ( !iterator.value().isMap() )
            {
                return fail(QStringLiteral("a live table-editor caret entry is not a map."));
            }
            const QString text = rows.at(row).toArray().at(column).toString();
            if ( !validateCaret(iterator.value().toMap(), text.size(), key) )
            {
                return false;
            }
        }
    }
    return true;
}

void ResourceAuthoringDialog::restoreEditorPresentation(const QCborMap& presentation)
{
    const bool wasApplying = applyingEditorPresentation_;
    applyingEditorPresentation_ = true;
    const QCborMap lineEdits = presentation.value(QStringLiteral("lineEdits")).toMap();
    const auto restoreCaret = [&lineEdits](const QString& key, QLineEdit* const edit)
    {
        if ( edit && lineEdits.value(key).isMap() )
        {
            restoreLineEditCaret(edit, lineEdits.value(key).toMap());
        }
    };
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        restoreCaret(QStringLiteral("spr.material"), spriteMaterial_);
        restoreCaret(QStringLiteral("spr.width"), spriteWidth_ ? spriteWidth_->lineEdit() : nullptr);
        restoreCaret(QStringLiteral("spr.height"), spriteHeight_ ? spriteHeight_->lineEdit() : nullptr);
        restoreCaret(QStringLiteral("spr.extra_light.scalar"),
            spriteExtraScalar_ ? spriteExtraScalar_->lineEdit() : nullptr);
        restoreCaret(QStringLiteral("spr.offset.x"), spriteOffset_[0] ? spriteOffset_[0]->lineEdit() : nullptr);
        restoreCaret(QStringLiteral("spr.offset.y"), spriteOffset_[1] ? spriteOffset_[1]->lineEdit() : nullptr);
        restoreCaret(QStringLiteral("spr.offset.z"), spriteOffset_[2] ? spriteOffset_[2]->lineEdit() : nullptr);
    }
    else if ( kind_ == ResourceAuthoringKind::soundClass )
    {
        restoreCaret(QStringLiteral("snd.search"), soundSearch_);
    }
    else
    {
        restoreCaret(QStringLiteral("uni.search"), stringSearch_);
    }

    pendingTableEditorCarets_ = presentation.value(QStringLiteral("tableEditors")).toMap();
    const QCborMap tables = presentation.value(QStringLiteral("tables")).toMap();
    deferredPresentationScroll_ = {};
    if ( kind_ == ResourceAuthoringKind::stringTable && stringTable_ )
    {
        const QCborValue tableValue = tables.value(QStringLiteral("uni.entries"));
        if ( tableValue.isMap() )
        {
            const QCborMap table = tableValue.toMap();
            const QCborValue selectionValue = table.value(QStringLiteral("selection"));
            QItemSelectionModel* const selectionModel = stringTable_->selectionModel();
            if ( selectionModel && selectionValue.isArray() )
            {
                selectionModel->clearSelection();
                QItemSelection selection{};
                for ( const QCborValue& rangeValue : selectionValue.toArray() )
                {
                    const QCborMap range = rangeValue.toMap();
                    const QModelIndex first = stringTable_->model()->index(
                        static_cast<int>(range.value(QStringLiteral("top")).toInteger()),
                        static_cast<int>(range.value(QStringLiteral("left")).toInteger()));
                    const QModelIndex last = stringTable_->model()->index(
                        static_cast<int>(range.value(QStringLiteral("bottom")).toInteger()),
                        static_cast<int>(range.value(QStringLiteral("right")).toInteger()));
                    if ( first.isValid() && last.isValid() )
                    {
                        selection.select(first, last);
                    }
                }
                if ( !selection.isEmpty() )
                {
                    selectionModel->select(selection, QItemSelectionModel::Select);
                }
            }
            const QCborValue current = table.value(QStringLiteral("current"));
            if ( selectionModel && current.isNull() )
            {
                selectionModel->setCurrentIndex(QModelIndex{}, QItemSelectionModel::NoUpdate);
            }
            else if ( selectionModel && current.isMap() )
            {
                const QCborMap identity = current.toMap();
                const QModelIndex currentIndex = stringTable_->model()->index(
                    static_cast<int>(identity.value(QStringLiteral("row")).toInteger()),
                    static_cast<int>(identity.value(QStringLiteral("column")).toInteger()));
                selectionModel->setCurrentIndex(currentIndex, QItemSelectionModel::NoUpdate);
            }
            const QCborValue horizontal = table.value(QStringLiteral("horizontalScroll"));
            const QCborValue vertical = table.value(QStringLiteral("verticalScroll"));
            if ( !horizontal.isUndefined() || !vertical.isUndefined() )
            {
                deferredPresentationScroll_.view = stringTable_;
                deferredPresentationScroll_.horizontal = horizontal.isInteger()
                    ? static_cast<int>(horizontal.toInteger()) : stringTable_->horizontalScrollBar()->value();
                deferredPresentationScroll_.vertical = vertical.isInteger()
                    ? static_cast<int>(vertical.toInteger()) : stringTable_->verticalScrollBar()->value();
                deferredPresentationScroll_.presentationGeneration = editorPresentationGeneration_;
                deferredPresentationScroll_.pending = true;
            }
        }
    }
    else if ( kind_ == ResourceAuthoringKind::soundClass && soundEntries_ )
    {
        const QCborValue tableValue = tables.value(QStringLiteral("snd.entries"));
        if ( tableValue.isMap() )
        {
            const QCborMap table = tableValue.toMap();
            soundManualColumnWidths_ = table.value(QStringLiteral("manualWidths")).toBool();
            const QCborValue widthsValue = table.value(QStringLiteral("widths"));
            if ( soundManualColumnWidths_ && widthsValue.isArray() && widthsValue.toArray().size() == 6 )
            {
                for ( int column = 0; column < 6; ++column )
                {
                    soundEntries_->setColumnWidth(
                        column, static_cast<int>(widthsValue.toArray().at(column).toInteger()));
                }
            }
            const QCborValue current = table.value(QStringLiteral("current"));
            if ( current.isNull() )
            {
                soundEntries_->setCurrentItem(nullptr, 0);
            }
            else if ( current.isMap() )
            {
                const QCborMap identity = current.toMap();
                const int itemKind = static_cast<int>(identity.value(QStringLiteral("kind")).toInteger());
                QTreeWidgetItem* target = nullptr;
                if ( itemKind == static_cast<int>(SoundTreeItemKind::alternative) )
                {
                    target = soundAlternativeAt(
                        static_cast<int>(identity.value(QStringLiteral("sourceOrdinal")).toInteger()));
                }
                else if ( identity.value(QStringLiteral("modeBytes")).isByteArray() )
                {
                    const QByteArray modeBytes = identity.value(QStringLiteral("modeBytes")).toByteArray();
                    const auto findMatch = [&modeBytes, itemKind](const auto& self, QTreeWidgetItem* const item)
                        -> QTreeWidgetItem*
                    {
                        if ( item->data(0, detail::kSoundTreeItemKindRole).toInt() == itemKind
                            && item->data(0, detail::kSoundTreeModeBytesRole).toByteArray() == modeBytes )
                        {
                            return item;
                        }
                        for ( int child = 0; child < item->childCount(); ++child )
                        {
                            if ( QTreeWidgetItem* const match = self(self, item->child(child)) )
                            {
                                return match;
                            }
                        }
                        return nullptr;
                    };
                    for ( int top = 0; top < soundEntries_->topLevelItemCount() && !target; ++top )
                    {
                        target = findMatch(findMatch, soundEntries_->topLevelItem(top));
                    }
                }
                if ( target && table.value(QStringLiteral("column")).isInteger() )
                {
                    soundEntries_->setCurrentItem(
                        target, static_cast<int>(table.value(QStringLiteral("column")).toInteger()));
                }
            }
            const QCborValue horizontal = table.value(QStringLiteral("horizontalScroll"));
            const QCborValue vertical = table.value(QStringLiteral("verticalScroll"));
            if ( !horizontal.isUndefined() || !vertical.isUndefined() )
            {
                deferredPresentationScroll_.view = soundEntries_;
                deferredPresentationScroll_.horizontal = horizontal.isInteger()
                    ? static_cast<int>(horizontal.toInteger()) : soundEntries_->horizontalScrollBar()->value();
                deferredPresentationScroll_.vertical = vertical.isInteger()
                    ? static_cast<int>(vertical.toInteger()) : soundEntries_->verticalScrollBar()->value();
                deferredPresentationScroll_.presentationGeneration = editorPresentationGeneration_;
                deferredPresentationScroll_.pending = true;
            }
        }
        else
        {
            soundManualColumnWidths_ = false;
        }
    }
    if ( stringTable_ )
    {
        stringTable_->doItemsLayout();
    }
    if ( soundEntries_ )
    {
        soundEntries_->doItemsLayout();
    }
    if ( deferredPresentationScroll_.pending )
    {
        deferredPresentationScroll_.interactionGeneration = presentationInteractionGeneration_;
    }
    applyingEditorPresentation_ = wasApplying;
    if ( deferredPresentationScroll_.pending )
    {
        QTimer::singleShot(0, this, [this]() { applyDeferredPresentationScroll(); });
    }
}

void ResourceAuthoringDialog::cancelDeferredEditorPresentation()
{
    ++editorPresentationGeneration_;
    pendingTableEditorCarets_.clear();
    deferredPresentationScroll_ = {};
}

void ResourceAuthoringDialog::applyDeferredPresentationScroll()
{
    if ( !deferredPresentationScroll_.pending )
    {
        return;
    }
    if ( !deferredPresentationScroll_.view
        || deferredPresentationScroll_.presentationGeneration != editorPresentationGeneration_
        || deferredPresentationScroll_.interactionGeneration != presentationInteractionGeneration_ )
    {
        deferredPresentationScroll_ = {};
        return;
    }
    if ( !deferredPresentationScroll_.view->isVisible() )
    {
        return;
    }
    const QPointer<QAbstractItemView> view = deferredPresentationScroll_.view;
    const int horizontal = deferredPresentationScroll_.horizontal;
    const int vertical = deferredPresentationScroll_.vertical;
    applyingPresentationScroll_ = true;
    view->doItemsLayout();
    if ( view )
    {
        view->horizontalScrollBar()->setValue(horizontal);
        view->verticalScrollBar()->setValue(vertical);
    }
    applyingPresentationScroll_ = false;
    deferredPresentationScroll_ = {};
}

void ResourceAuthoringDialog::restorePendingTableEditorCaret(
    QAbstractItemView* const view,
    QWidget* const editor,
    const QModelIndex& index)
{
    const QString key = tableEditorPresentationKey(view, index);
    const QCborValue caretValue = pendingTableEditorCarets_.value(key);
    QLineEdit* const edit = lineEditForTableEditor(editor);
    if ( key.isEmpty() || !caretValue.isMap() || !edit )
    {
        return;
    }
    const std::uint64_t generation = editorPresentationGeneration_;
    const std::uint64_t inputGeneration = presentationInputGeneration_;
    const QPointer<QAbstractItemView> guardedView(view);
    const QPointer<QWidget> guardedEditor(editor);
    const QPointer<QLineEdit> guardedLineEdit(edit);
    const QPersistentModelIndex guardedIndex(index);
    QTimer::singleShot(0, editor, [this, key, generation, inputGeneration, guardedView, guardedEditor, guardedLineEdit,
                                  guardedIndex, caret = caretValue.toMap()]()
    {
        if ( generation != editorPresentationGeneration_ || inputGeneration != presentationInputGeneration_
            || !guardedView || !guardedEditor
            || !guardedLineEdit || !guardedIndex.isValid()
            || guardedView->indexWidget(guardedIndex) != guardedEditor.data()
            || !pendingTableEditorCarets_.contains(key) )
        {
            return;
        }
        restoreLineEditCaret(guardedLineEdit.data(), caret);
        pendingTableEditorCarets_.remove(key);
    });
}

bool ResourceAuthoringDialog::validateState(const QCborMap& state, QString* warning)
{
    const auto fail = [warning](const QString& message)
    {
        if ( warning )
        {
            *warning = message;
        }
        return false;
    };
    const QCborValue version = state.value(QStringLiteral("version"));
    const QCborValue kind = state.value(QStringLiteral("kind"));
    const QCborValue sourceKind = state.value(QStringLiteral("sourceKind"));
    const QCborValue profile = state.value(QStringLiteral("profile"));
    const QCborValue name = state.value(QStringLiteral("name"));
    const QCborValue path = state.value(QStringLiteral("path"));
    const QCborValue resource = state.value(QStringLiteral("projectResource"));
    const QCborValue archiveTooltip = state.value(QStringLiteral("archiveTooltip"));
    const QCborValue draftId = state.value(QStringLiteral("draftIdentity"));
    const QCborValue modified = state.value(QStringLiteral("modified"));
    const QCborValue diskHash = state.value(QStringLiteral("diskHash"));
    const QCborValue paneGeometry = state.value(QStringLiteral("paneGeometry"));
    const QCborValue search = state.value(QStringLiteral("search"));
    if ( !version.isInteger() || version.toInteger() != 1 || !kind.isInteger() || kind.toInteger() < 0 ||
         kind.toInteger() > 2 || !sourceKind.isInteger() || sourceKind.toInteger() < 0 || sourceKind.toInteger() > 2 ||
         !profile.isInteger() || profile.toInteger() < 0 || profile.toInteger() > 1 || !name.isString() ||
         name.toString().size() > 4096 || !path.isString() || path.toString().size() > 32768 || !resource.isString() ||
         resource.toString().size() > 4096 || !draftId.isString() || draftId.toString().size() > 128 ||
         (!archiveTooltip.isUndefined() &&
          (!archiveTooltip.isString() || !validArchiveSourceTooltip(archiveTooltip.toString()))) ||
         !modified.isBool() || !diskHash.isByteArray() || !paneGeometry.isByteArray() ||
         paneGeometry.toByteArray().size() > 1024 * 1024 || !search.isString() || search.toString().size() > 4096 ||
         (!diskHash.toByteArray().isEmpty() && diskHash.toByteArray().size() != 32) )
    {
        return fail(QStringLiteral("Resource draft metadata is invalid."));
    }
    const int sourceKindValue = static_cast<int>(sourceKind.toInteger());
    const QString sourcePath = path.toString();
    const QString resourceName = resource.toString();
    const QString archiveTooltipText = archiveTooltip.toString();
    const QString draftName = name.toString();
    const QString draftIdentity = draftId.toString();
    const QByteArray diskHashBytes = diskHash.toByteArray();
    if ( sourcePath.contains(QChar::Null) || resourceName.contains(QChar::Null) || draftName.contains(QChar::Null) ||
         draftName.contains(QLatin1Char('/')) || draftName.contains(QLatin1Char('\\')) ||
         (sourceKindValue == static_cast<int>(SourceKind::loose) &&
          (sourcePath.isEmpty() || !resourceName.isEmpty() || !draftIdentity.isEmpty() ||
           diskHashBytes.size() != 32)) ||
         (sourceKindValue == static_cast<int>(SourceKind::projectResource) &&
          (!sourcePath.isEmpty() || resourceName.isEmpty() || !draftIdentity.isEmpty() ||
           diskHashBytes.size() != 32)) ||
         (sourceKindValue != static_cast<int>(SourceKind::projectResource) &&
          !archiveTooltipText.isEmpty()) ||
         (sourceKindValue == static_cast<int>(SourceKind::draft) &&
          (!sourcePath.isEmpty() || !resourceName.isEmpty() || draftIdentity.isEmpty() || !diskHashBytes.isEmpty())) )
    {
        return fail(QStringLiteral("Resource source identity is invalid."));
    }
    const int kindValue = static_cast<int>(kind.toInteger());
    const int profileValue = static_cast<int>(profile.toInteger());
    const QCborValue editorPresentation = state.value(QStringLiteral("editorPresentation"));
    if ( !editorPresentation.isUndefined() )
    {
        if ( !editorPresentation.isMap() )
        {
            if ( warning )
            {
                *warning = QStringLiteral("Resource editor presentation map is invalid.");
            }
            return false;
        }
        if ( !validateEditorPresentation(editorPresentation.toMap(), state, warning) )
        {
            return false;
        }
    }
    if ( (kindValue == static_cast<int>(ResourceAuthoringKind::sprite) &&
          profileValue != static_cast<int>(assets::SprProfile::classic) &&
          profileValue != static_cast<int>(assets::SprProfile::ijim)) ||
         (kindValue == static_cast<int>(ResourceAuthoringKind::soundClass) &&
          profileValue != static_cast<int>(assets::SoundClassProfile::classic) &&
          profileValue != static_cast<int>(assets::SoundClassProfile::ijim)) ||
         (kindValue == static_cast<int>(ResourceAuthoringKind::stringTable) &&
          profileValue != static_cast<int>(assets::UniProfile::classic) &&
          profileValue != static_cast<int>(assets::UniProfile::ijim)) )
    {
        return fail(QStringLiteral("Resource profile does not match its editor kind."));
    }
    if ( kind.toInteger() == static_cast<int>(ResourceAuthoringKind::sprite) )
    {
        if ( !state.value(QStringLiteral("soundEmptyModes")).isUndefined() )
        {
            return fail(QStringLiteral("Sound empty modes are not valid for a Sprite resource."));
        }
        const QCborValue pendingValue = state.value(QStringLiteral("spritePendingNumericEdits"));
        if ( !pendingValue.isUndefined() )
        {
            static const QSet<QString> allowedPendingKeys{
                QStringLiteral("width"),   QStringLiteral("height"),  QStringLiteral("extraLightScalar"),
                QStringLiteral("offsetX"), QStringLiteral("offsetY"), QStringLiteral("offsetZ"),
            };
            if ( !pendingValue.isMap() || pendingValue.toMap().size() > 32 )
            {
                return fail(QStringLiteral("Sprite pending numeric edits are invalid."));
            }
            const QCborMap pending = pendingValue.toMap();
            for ( auto iterator = pending.cbegin(); iterator != pending.cend(); ++iterator )
            {
                const QCborValue keyValue = iterator.key();
                const QString key = keyValue.toString();
                if ( !keyValue.isString() || key.trimmed().isEmpty() || key.size() > 128 || key.contains(QChar::Null) )
                {
                    return fail(QStringLiteral("Sprite pending numeric edit has an invalid key or value."));
                }
                if ( !allowedPendingKeys.contains(key) )
                {
                    if ( iterator.value().toCbor().size() > 4096 )
                    {
                        return fail(QStringLiteral("Unknown Sprite pending numeric data exceeds its size limit."));
                    }
                    continue;
                }
                if ( !iterator.value().isString() || iterator.value().toString().size() > 4096 ||
                     iterator.value().toString().contains(QChar::Null) )
                {
                    return fail(QStringLiteral("Sprite pending numeric edit has an invalid key or value."));
                }
            }
        }
        const QCborValue presentation = state.value(QStringLiteral("presentation"));
        if ( !presentation.isUndefined() &&
             (!presentation.isMap() || !validateSpritePresentation(presentation.toMap(), warning)) )
        {
            return false;
        }
        const QCborValue sprite = state.value(QStringLiteral("sprite"));
        if ( !sprite.isArray() || sprite.toArray().size() != 15 )
        {
            return fail(QStringLiteral("Sprite draft values are invalid."));
        }
        const QCborArray values = sprite.toArray();
        if ( !values.at(0).isString() || !values.at(1).isInteger() || !values.at(2).isString() ||
             !values.at(3).isString() || !values.at(4).isInteger() || !values.at(5).isInteger() ||
             !values.at(6).isInteger() || !values.at(7).isString() )
        {
            return fail(QStringLiteral("Sprite draft values have an invalid shape."));
        }
        for ( const qsizetype index : {qsizetype{1}, qsizetype{4}, qsizetype{5}, qsizetype{6}} )
        {
            const qint64 value = values.at(index).toInteger();
            if ( value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max() )
            {
                return fail(QStringLiteral("Sprite integer draft value is outside the supported range."));
            }
        }
        for ( qsizetype index = 8; index < 15; ++index )
        {
            if ( !values.at(index).isString() )
            {
                return fail(QStringLiteral("Sprite numeric draft value has an invalid type."));
            }
        }
        if ( values.at(0).toString().size() > 4096 )
        {
            return fail(QStringLiteral("Sprite material exceeds its text limit."));
        }
        for ( const qsizetype index : {qsizetype{2}, qsizetype{3}, qsizetype{7}, qsizetype{8}, qsizetype{9},
                                       qsizetype{10}, qsizetype{11}, qsizetype{12}, qsizetype{13}, qsizetype{14}} )
        {
            if ( !parseFiniteFloat(values.at(index).toString()) )
            {
                return fail(QStringLiteral("Sprite numeric draft value is invalid."));
            }
        }
    }
    else
    {
        if ( !state.value(QStringLiteral("spritePendingNumericEdits")).isUndefined() )
        {
            return fail(QStringLiteral("Sprite pending numeric data is not valid for this resource kind."));
        }
        const QCborValue rowsValue = state.value(QStringLiteral("rows"));
        const QCborArray rows = rowsValue.toArray();
        const int expectedColumns = kind.toInteger() == static_cast<int>(ResourceAuthoringKind::soundClass) ? 6 : 3;
        if ( !rowsValue.isArray() || rows.size() > 4096 )
        {
            return fail(QStringLiteral("Resource draft row count exceeds its limit."));
        }
        for ( const QCborValue& rowValue : rows )
        {
            if ( !rowValue.isArray() || rowValue.toArray().size() != expectedColumns )
            {
                return fail(QStringLiteral("Resource draft row shape is invalid."));
            }
            for ( const QCborValue& cell : rowValue.toArray() )
            {
                if ( !cell.isString() || cell.toString().size() > 4096 )
                {
                    return fail(QStringLiteral("Resource draft cell exceeds its limit."));
                }
            }
        }
        if ( kind.toInteger() == static_cast<int>(ResourceAuthoringKind::stringTable) )
        {
            if ( !state.value(QStringLiteral("soundEmptyModes")).isUndefined() )
            {
                return fail(QStringLiteral("Sound empty modes are not valid for a UNI resource."));
            }
            if ( !state.value(QStringLiteral("soundPreviewLoops")).isUndefined() )
            {
                return fail(QStringLiteral("Sound preview loop state is not valid for a UNI resource."));
            }
            const QCborValue capacity = state.value(QStringLiteral("capacity"));
            if ( !capacity.isInteger() || capacity.toInteger() < 0 || capacity.toInteger() > 4096 )
            {
                return fail(QStringLiteral("String-table draft capacity is invalid."));
            }
        }
        else
        {
            const QCborValue emptyModes = state.value(QStringLiteral("soundEmptyModes"));
            const auto modeNames = assets::soundClassModeNames(static_cast<assets::SoundClassProfile>(profileValue));
            if ( !emptyModes.isUndefined() )
            {
                if ( !emptyModes.isArray() || emptyModes.toArray().size() > static_cast<qsizetype>(modeNames.size()) )
                {
                    return fail(QStringLiteral("Sound empty-mode presentation exceeds its limit."));
                }
                QSet<QString> seen;
                for ( const QCborValue& value : emptyModes.toArray() )
                {
                    const QString mode = value.toString();
                    const std::string bytes = mode.toStdString();
                    if ( !value.isString() || seen.contains(mode) ||
                         std::find(modeNames.begin(), modeNames.end(), bytes) == modeNames.end() )
                    {
                        return fail(QStringLiteral("Sound empty-mode presentation contains an invalid mode."));
                    }
                    for ( const QCborValue& row : rows )
                    {
                        if ( row.toArray().at(0).toString() == mode )
                        {
                            return fail(QStringLiteral("Sound empty-mode presentation duplicates an authored mode."));
                        }
                    }
                    seen.insert(mode);
                }
            }
            const QCborValue collapsed = state.value(QStringLiteral("collapsedModes"));
            if ( !collapsed.isArray() || collapsed.toArray().size() > rows.size() + emptyModes.toArray().size() )
            {
                return fail(QStringLiteral("Sound-mode collapse state is invalid."));
            }
            for ( const QCborValue& mode : collapsed.toArray() )
            {
                if ( !mode.isString() || mode.toString().size() > 4096 )
                {
                    return fail(QStringLiteral("Sound-mode collapse label is invalid."));
                }
            }
            const QCborValue loops = state.value(QStringLiteral("soundPreviewLoops"));
            if ( !loops.isUndefined() )
            {
                if ( !loops.isArray() || loops.toArray().size() != rows.size() )
                {
                    return fail(QStringLiteral("Sound preview loop state does not match the source rows."));
                }
                for ( const QCborValue& loop : loops.toArray() )
                {
                    if ( !loop.isBool() )
                    {
                        return fail(QStringLiteral("Sound preview loop state contains a non-boolean value."));
                    }
                }
            }
        }
    }
    if ( !validateDocumentHistory(state) )
    {
        return fail(QStringLiteral("Resource document history is invalid."));
    }
    return true;
}

bool ResourceAuthoringDialog::eventFilter(QObject* watched, QEvent* event)
{
    QWidget* const eventWidget = qobject_cast<QWidget*>(watched);
    const auto belongsTo = [eventWidget](const QWidget* const root)
    {
        return eventWidget && root && (eventWidget == root || root->isAncestorOf(eventWidget));
    };
    const bool presentationTarget = belongsTo(soundEntries_) || belongsTo(soundSearch_)
        || belongsTo(stringTable_) || belongsTo(stringSearch_);
    const bool presentationInput = event &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick
            || event->type() == QEvent::KeyPress || event->type() == QEvent::Wheel
            || event->type() == QEvent::TouchBegin);
    if ( presentationTarget && presentationInput && !refreshing_ && !applyingEditorPresentation_
        && !applyingPresentationScroll_ )
    {
        ++presentationInteractionGeneration_;
        ++presentationInputGeneration_;
        const auto clearEditedCellCaret = [this, eventWidget](QAbstractItemView* const view)
        {
            if ( !view || !eventWidget )
            {
                return;
            }
            const QModelIndex current = view->currentIndex();
            QWidget* const editor = current.isValid() ? view->indexWidget(current) : nullptr;
            if ( editor && (eventWidget == editor || editor->isAncestorOf(eventWidget)) )
            {
                pendingTableEditorCarets_.remove(tableEditorPresentationKey(view, current));
            }
        };
        clearEditedCellCaret(soundEntries_);
        clearEditedCellCaret(stringTable_);
    }
    if ( auto* const line = qobject_cast<QLineEdit*>(watched); line && event && !refreshing_ )
    {
        const QString numericKey = line->property("resource_authoring.sprite_numeric_key").toString();
        const auto* const keyEvent = event->type() == QEvent::KeyPress ? static_cast<QKeyEvent*>(event) : nullptr;
        const bool committingText =
            event->type() == QEvent::FocusOut ||
            (keyEvent && (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter));
        if ( committingText && spriteNumericUndoKeys_.contains(numericKey) &&
             !spritePendingNumericEdits_.contains(numericKey) )
        {
            if ( const auto canonical = spriteNumericValue(numericKey) )
            {
                // Undo restores the original display, which can round a tiny
                // source float to zero. Commit the canonical float unchanged.
                const QSignalBlocker blocker(line);
                QString text = floatText(*canonical);
                text.replace(QLatin1Char('.'), line->locale().decimalPoint());
                line->setText(text);
            }
            spriteNumericEditBaselines_.remove(numericKey);
            spriteNumericUndoKeys_.remove(numericKey);
        }
        if ( !numericKey.isEmpty() && (event->type() == QEvent::FocusIn || event->type() == QEvent::KeyPress) )
        {
            if ( !spritePendingNumericEdits_.contains(numericKey) && !spriteNumericEditBaselines_.contains(numericKey) )
            {
                spriteNumericEditBaselines_.insert(numericKey, line->text());
            }
            if ( event->type() == QEvent::KeyPress )
            {
                const auto* const key = static_cast<QKeyEvent*>(event);
                line->setProperty("resource_authoring.sprite_undo_request", key->matches(QKeySequence::Undo));
            }
        }
    }
    if ( soundEntries_ && watched == soundEntries_ && event &&
         (event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate ||
          event->type() == QEvent::ApplicationDeactivate) )
    {
        if ( handleSoundTreePointerEvent(event) )
        {
            return true;
        }
    }
    if ( soundEntries_ && event &&
         (watched == soundPlayButton_ || watched == soundLoopButton_ || watched == soundRemoveButton_ ||
          watched == soundPlayingStopButton_ || watched == soundPlayingLoopButton_) )
    {
        if ( event->type() == QEvent::Enter )
        {
            const auto* const enter = static_cast<QEnterEvent*>(event);
            updateSoundHoverControls(soundEntries_->viewport()->mapFromGlobal(enter->globalPosition().toPoint()));
        }
        else if ( event->type() == QEvent::MouseMove )
        {
            const auto* const mouse = static_cast<QMouseEvent*>(event);
            updateSoundHoverControls(soundEntries_->viewport()->mapFromGlobal(mouse->globalPosition().toPoint()));
        }
    }
    if ( soundEntries_ && watched == soundEntries_->viewport() && event )
    {
        if ( handleSoundTreePointerEvent(event) )
        {
            return true;
        }
        if ( event->type() == QEvent::MouseMove )
        {
            updateSoundHoverControls(static_cast<QMouseEvent*>(event)->position().toPoint());
        }
        else if ( event->type() == QEvent::Resize )
        {
            const auto* const resize = static_cast<QResizeEvent*>(event);
            const QPointer<ResourceAuthoringDialog> self(this);
            // Showing a horizontal scrollbar changes height only. Preserve
            // deliberately wider sections instead of compacting that scroll.
            resizeSoundWavColumn(resize->oldSize().width() != resize->size().width(), 5);
            if ( !self )
            {
                return true;
            }
            if ( soundHoveredIndex_.isValid() )
            {
                updateSoundHoverControls(soundEntries_->visualRect(soundHoveredIndex_).center());
            }
            updateSoundPlayingControls();
        }
    }
    if ( soundEntries_ && event && event->type() == QEvent::Leave &&
         (watched == soundEntries_->viewport() || watched == soundPlayButton_ || watched == soundLoopButton_ ||
          watched == soundRemoveButton_ || watched == soundPlayingStopButton_ || watched == soundPlayingLoopButton_) )
    {
        // A viewport Leave precedes Enter on its child buttons. Resolve the
        // final position after that transition so the hovered control stays live.
        const QPointer<ResourceAuthoringDialog> self(this);
        QTimer::singleShot(0, this,
                           [self]()
                           {
                               if ( !self || !self->soundEntries_ )
                               {
                                   return;
                               }
                               QWidget* const viewport = self->soundEntries_->viewport();
                               const QPoint local = viewport->mapFromGlobal(QCursor::pos());
                               if ( viewport->rect().contains(local) )
                               {
                                   self->updateSoundHoverControls(local);
                               }
                               else
                               {
                                   self->hideSoundHoverControls();
                               }
                           });
    }
    if ( watched == soundEntries_ && event && event->type() == QEvent::KeyPress )
    {
        auto* const key = static_cast<QKeyEvent*>(event);
        if ( key->matches(QKeySequence::Undo) && undoRedoDocument(false, true) )
        {
            return true;
        }
        if ( (key->matches(QKeySequence::Redo) ||
              (key->key() == Qt::Key_Z && key->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))) &&
             undoRedoDocument(true, true) )
        {
            return true;
        }
    }
    if ( event && event->type() == QEvent::KeyPress )
    {
        auto* const key = static_cast<QKeyEvent*>(event);
        const QString pendingKey =
            qobject_cast<QWidget*>(watched)
                ? qobject_cast<QWidget*>(watched)->property("resource_authoring.sprite_numeric_key").toString()
                : QString{};
        if ( key->key() == Qt::Key_Escape && !pendingKey.isEmpty() )
        {
            QPointer<ResourceAuthoringDialog> self(this);
            // This filter precedes the shared scrub filter. Roll back its
            // gesture before reading the accepted Sprite value, then own
            // Escape so QDialog cannot turn field cancellation into Close.
            if ( auto* const line = qobject_cast<QLineEdit*>(watched) )
            {
                if ( auto* const control = qobject_cast<ui::ScrubFloatEdit*>(line->parentWidget()) )
                {
                    control->cancelActiveGesture();
                }
            }
            if ( !self )
            {
                return true;
            }
            QTimer::singleShot(
                0, self.data(),
                [self, pendingKey]()
                {
                    if ( !self )
                    {
                        return;
                    }
                    ui::ScrubFloatEdit* control = nullptr;
                    if ( pendingKey == QStringLiteral("width") && self->spriteWidth_ )
                    {
                        control = self->spriteWidth_;
                    }
                    else if ( pendingKey == QStringLiteral("height") && self->spriteHeight_ )
                    {
                        control = self->spriteHeight_;
                    }
                    else if ( pendingKey == QStringLiteral("extraLightScalar") && self->spriteExtraScalar_ )
                    {
                        control = self->spriteExtraScalar_;
                    }
                    else if ( pendingKey == QStringLiteral("offsetX") || pendingKey == QStringLiteral("offsetY") ||
                              pendingKey == QStringLiteral("offsetZ") )
                    {
                        const int index = pendingKey == QStringLiteral("offsetX")   ? 0
                                          : pendingKey == QStringLiteral("offsetY") ? 1
                                                                                    : 2;
                        if ( self->spriteOffset_[static_cast<std::size_t>(index)] )
                        {
                            control = self->spriteOffset_[static_cast<std::size_t>(index)];
                        }
                    }
                    if ( const auto value = self->spriteNumericValue(pendingKey); control && value )
                    {
                        const bool wasRefreshing = self->refreshing_;
                        self->refreshing_ = true;
                        // Reset the formatter's settled baseline as well as its
                        // text so a later policy change can refresh this control.
                        control->setValue(*value);
                        self->refreshing_ = wasRefreshing;
                    }
                    if ( self->clearSpritePendingNumericEdit(pendingKey) )
                    {
                        self->markDirty();
                    }
                });
            return true;
        }
    }
    if ( extraLightPopup_ &&
         (watched == extraLightPopup_.data() || extraLightPopup_->isAncestorOf(qobject_cast<QWidget*>(watched))) &&
         event && event->type() == QEvent::KeyPress )
    {
        auto* const key = static_cast<QKeyEvent*>(event);
        if ( key->key() == Qt::Key_Escape )
        {
            extraLightPopupEscaped_ = true;
            extraLightPopup_->close();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void ResourceAuthoringDialog::closeEvent(QCloseEvent* event)
{
    QPointer<ResourceAuthoringDialog> self(this);
    if ( property("resource_authoring.force_close").toBool() )
    {
        event->accept();
        return;
    }
    const bool allowClose = confirmReplaceDocument();
    if ( !self )
    {
        return;
    }
    if ( allowClose )
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void ResourceAuthoringDialog::accept()
{
    // Return commits the active field; saving is always an explicit action.
}

void ResourceAuthoringDialog::reject()
{
    QPointer<ResourceAuthoringDialog> self(this);
    const bool allowReject = confirmReplaceDocument();
    if ( self && allowReject )
    {
        QDialog::reject();
    }
}

void ResourceAuthoringDialog::trackTableEditor(QAbstractItemView* const view, QWidget* const editor,
                                               const QModelIndex& index)
{
    if ( !view || !editor || !index.isValid() )
    {
        return;
    }
    editor->installEventFilter(this);
    for ( QObject* const child : editor->findChildren<QObject*>() )
    {
        child->installEventFilter(this);
    }
    pruneDetachedTableEditors();
    const auto existing = std::find_if(liveTableEditors_.begin(), liveTableEditors_.end(),
                                       [editor](const LiveTableEditor& entry)
                                       {
                                           return entry.editor.data() == editor;
                                       });
    if ( existing == liveTableEditors_.end() )
    {
        liveTableEditors_.push_back(LiveTableEditor{view, editor, QPersistentModelIndex(index)});
    }
    else
    {
        existing->view = view;
        existing->index = QPersistentModelIndex(index);
    }
    restorePendingTableEditorCaret(view, editor, index);

    QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
    if ( auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(editor) )
    {
        lineEdit = scrub->lineEdit();
    }
    else if ( !lineEdit )
    {
        lineEdit = editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text"));
    }
    if ( lineEdit )
    {
        const QPersistentModelIndex trackedIndex(index);
        const QPointer<QAbstractItemView> trackedView(view);
        const QPointer<QWidget> trackedEditor(editor);
        connect(lineEdit, &QLineEdit::textEdited, this,
                [this, trackedIndex, trackedView, trackedEditor, lineEdit](const QString& raw)
                {
                    if ( !trackedView || !trackedEditor || !trackedIndex.isValid() ||
                         trackedView->indexWidget(trackedIndex) != trackedEditor.data() )
                    {
                        return;
                    }
                    if ( trackedView.data() == soundEntries_ && trackedIndex.column() == 5 )
                    {
                        updateSoundPreviewVolume(
                            trackedIndex.sibling(trackedIndex.row(), 0).data(kSoundTreeSourceIndexRole).toInt(),
                            ui::canonicalUiNumericText(raw, lineEdit->locale()));
                    }
                    markDirty();
                });
    }
    else if ( auto* const combo = qobject_cast<QComboBox*>(editor) )
    {
        if ( combo->lineEdit() )
        {
            connect(combo->lineEdit(), &QLineEdit::textEdited, this,
                    [this](const QString&)
                    {
                        markDirty();
                    });
        }
        connect(combo, &QComboBox::textActivated, this,
                [this](const QString&)
                {
                    markDirty();
                });
    }
    if ( QSlider* const slider = editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) )
    {
        const QPointer<QAbstractItemView> trackedView(view);
        const QPointer<QWidget> trackedEditor(editor);
        const QPersistentModelIndex trackedIndex(index);
        connect(slider, &QSlider::valueChanged, this,
                [this, trackedView, trackedEditor, trackedIndex](int)
                {
                    if ( !trackedView || !trackedEditor || !trackedIndex.isValid() ||
                         trackedView->indexWidget(trackedIndex) != trackedEditor.data() )
                    {
                        return;
                    }
                    // The delegate updates raw text first. Publish that pending
                    // edit now; release records the single row history gesture.
                    if ( trackedView.data() == soundEntries_ && trackedIndex.column() == 5 )
                    {
                        if ( QLineEdit* const text =
                                 trackedEditor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) )
                        {
                            updateSoundPreviewVolume(
                                trackedIndex.sibling(trackedIndex.row(), 0).data(kSoundTreeSourceIndexRole).toInt(),
                                ui::canonicalUiNumericText(text->text(), text->locale()));
                        }
                    }
                    markDirty();
                });
    }
}

void ResourceAuthoringDialog::pruneDetachedTableEditors()
{
    std::erase_if(liveTableEditors_,
                  [](const LiveTableEditor& entry)
                  {
                      return !entry.view || !entry.editor || !entry.index.isValid() ||
                             entry.view->indexWidget(entry.index) != entry.editor.data();
                  });
}

QString ResourceAuthoringDialog::cellTextWithPendingEditor(const QAbstractItemView* const view,
                                                           const QModelIndex& index, const QString& fallback) const
{
    if ( view && index.isValid() )
    {
        for ( auto iterator = liveTableEditors_.rbegin(); iterator != liveTableEditors_.rend(); ++iterator )
        {
            if ( iterator->view.data() != view || iterator->index != index || !iterator->editor ||
                 view->indexWidget(iterator->index) != iterator->editor.data() )
            {
                continue;
            }
            // Localized settled presentation is not an authored raw row. Only
            // an actual pending numeric edit contributes its exact UI text.
            if ( view == soundEntries_ && index.column() >= 3 && index.column() <= 5
                && iterator->editor->property("snd.numeric.originalText").isValid()
                && !iterator->editor->property("snd.numeric.pending").toBool() )
            {
                return iterator->editor->property("snd.numeric.originalText").toString();
            }
            if ( auto* const lineEdit = qobject_cast<QLineEdit*>(iterator->editor.data()) )
            {
                return lineEdit->text();
            }
            if ( auto* const combo = qobject_cast<QComboBox*>(iterator->editor.data()) )
            {
                return combo->currentText();
            }
            if ( auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(iterator->editor.data()) )
            {
                return scrub->lineEdit()->text();
            }
            if ( QLineEdit* const text = iterator->editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) )
            {
                return text->text();
            }
        }
    }
    return fallback;
}

void ResourceAuthoringDialog::settleActiveTableEditors()
{
    if ( settlingTableEditors_ )
    {
        return;
    }
    pruneDetachedTableEditors();
    settlingTableEditors_ = true;
    QPointer<ResourceAuthoringDialog> self(this);
    const std::vector<LiveTableEditor> editors = liveTableEditors_;
    for ( const LiveTableEditor& entry : editors )
    {
        if ( entry.view && entry.editor && entry.index.isValid() )
        {
            (void)QMetaObject::invokeMethod(
                entry.view.data(),
                "commitData",
                Qt::DirectConnection,
                Q_ARG(QWidget*, entry.editor.data()));
            if ( !self )
            {
                return;
            }
        }
    }
    settlingTableEditors_ = false;
}

void ResourceAuthoringDialog::changed()
{
    notifyChanged();
}

void ResourceAuthoringDialog::buildCommonChrome()
{
    auto* const root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto* const actions = new QHBoxLayout();
    newButton_ = makeToolbarButton(
        ui::makeViewportIcon(ui::ViewportIcon::resourceNew),
        QStringLiteral("New resource"),
        "resource_authoring.new");
    openButton_ = makeToolbarButton(
        ui::makeViewportIcon(ui::ViewportIcon::resourceOpen),
        QStringLiteral("Open resource from the active project"),
        "resource_authoring.open");
    saveButton_ = makeToolbarButton(
        ui::makeViewportIcon(ui::ViewportIcon::actionSave),
        QStringLiteral("Save resource"),
        "resource_authoring.save");

    auto* const saveAsAction = new QAction(QStringLiteral("Save As…"), this);
    saveAsAction->setObjectName(QStringLiteral("resource_authoring.save_as"));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    saveAsAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(saveAsAction);

    for ( QToolButton* const button : { newButton_, openButton_, saveButton_ } )
    {
        if ( embedded_ && context_.groupedWorkspaceChrome )
        {
            button->hide();
        }
        else
        {
            actions->addWidget(button);
        }
    }
    if ( actions->isEmpty() )
    {
        delete actions;
    }
    else
    {
        root->addLayout(actions);
    }

    contentHost_ = new QWidget(this);
    contentHost_->setObjectName(QStringLiteral("resource_authoring.content"));
    contentLayout_ = new QVBoxLayout(contentHost_);
    contentLayout_->setContentsMargins(0, 0, 0, 0);
    root->addWidget(contentHost_, 1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("resource_authoring.status"));
    statusLabel_->setMinimumHeight(24);
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    connect(newButton_, &QToolButton::clicked, this, [this]()
        {
            if ( embedded_ && context_.createNewDraft )
            {
                (void)context_.createNewDraft(this, kind_);
                return;
            }
            createNewDocument();
        });
    connect(openButton_, &QToolButton::clicked, this, [this]()
        {
            if ( embedded_ )
            {
                (void)openProjectResource();
                return;
            }
            (void)openDocument();
        });
    connect(saveButton_, &QToolButton::clicked, this, [this]()
        {
            (void)saveDocument(false);
        });
    connect(saveAsAction, &QAction::triggered, this, [this]()
        {
            (void)saveDocument(true);
        });
}

QToolButton* ResourceAuthoringDialog::makeToolbarButton(
    const QIcon& icon,
    const QString& tooltip,
    const char* objectName)
{
    auto* const button = new QToolButton(this);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setIcon(icon);
    button->setIconSize(QSize(20, 20));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(30, 30);
    return button;
}

QPushButton* ResourceAuthoringDialog::makeButton(const QString& label, const char* objectName)
{
    auto* const button = new QPushButton(label, this);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setAutoDefault(false);
    button->setDefault(false);
    return button;
}

void ResourceAuthoringDialog::selectIntegerCombo(QComboBox* combo, const int value)
{
    int index = combo->findData(value);
    if ( index < 0 )
    {
        combo->addItem(QStringLiteral("Preserved value (%1)").arg(value), value);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index);
}

void ResourceAuthoringDialog::refreshDocumentWidgets()
{
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        refreshSpriteWidgets();
        scheduleSpritePreview(true);
    }
    else if ( kind_ == ResourceAuthoringKind::soundClass )
    {
        refreshSoundRows();
    }
    else
    {
        refreshStringRows();
    }
    resetDocumentHistory();
    updateDocumentLabel();
}

void ResourceAuthoringDialog::initializeNewDocument(const bool asDraft)
{
    cancelDeferredEditorPresentation();
    soundManualColumnWidths_ = false;
    switch ( kind_ )
    {
        case ResourceAuthoringKind::sprite:
            spritePendingNumericEdits_.clear();
            spriteNumericEditBaselines_.clear();
            spriteNumericUndoKeys_.clear();
            sprite_ = {};
            sprite_.material = "new_sprite.mat";
            sprite_.type = 0;
            sprite_.width = 1.0f;
            sprite_.height = 1.0f;
            sprite_.geometryMode = static_cast<int>(TGeoMode::Texture);
            sprite_.lightMode = static_cast<int>(TLightMode::Gouraud);
            sprite_.textureMode = 0;
            sprite_.extraLightRGBA = { 0.0f, 0.0f, 0.0f, 1.0f };
            sprite_.offsetXYZ = { 0.0f, 0.0f, 0.0f };
            spriteProfile_ = defaultSpriteProfile(context_.projectType);
            break;
        case ResourceAuthoringKind::soundClass:
            soundFile_ = {};
            soundRawRows_.clear();
            soundPreviewLoopRows_.clear();
            knownSoundModes_.clear();
            soundPresentationModes_.clear();
            collapsedSoundModes_.clear();
            break;
        case ResourceAuthoringKind::stringTable:
            stringsFile_ = {};
            break;
    }
    currentPath_.clear();
    diskBaselineHash_.clear();
    sourceKind_ = SourceKind::draft;
    projectResourceName_.clear();
    archiveSourceTooltip_.clear();
    draftName_ = suggestedBaseName();
    draftIdentity_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dirty_ = asDraft;
    resetSoundHistory();
    ++authoredRevision_;
    updateDocumentLabel();
    setStatus(QStringLiteral("Ready."));
    refreshDocumentWidgets();
    if ( asDraft )
    {
        notifyChanged();
    }
}

void ResourceAuthoringDialog::createNewDocument()
{
    QPointer<ResourceAuthoringDialog> self(this);
    if ( !self->confirmReplaceDocument() || !self )
    {
        return;
    }
    self->initializeNewDocument(true);
}

bool ResourceAuthoringDialog::confirmReplaceDocument(bool* discardedAny)
{
    if ( discardedAny )
    {
        *discardedAny = false;
    }
    if ( !dirty_ )
    {
        return true;
    }
    QPointer<ResourceAuthoringDialog> self(this);
    const std::uint64_t revision = authoredRevision_;
    const QCborMap beforePrompt = authoredContentState();
    QString promptTitle{};
    switch ( kind_ )
    {
        case ResourceAuthoringKind::sprite:
            promptTitle = QStringLiteral("Unsaved sprite");
            break;
        case ResourceAuthoringKind::soundClass:
            promptTitle = QStringLiteral("Unsaved sound class");
            break;
        case ResourceAuthoringKind::stringTable:
            promptTitle = QStringLiteral("Unsaved string table");
            break;
    }
    const QMessageBox::StandardButton result = ui::components::showSharedUnsavedChangesPrompt(
        this, promptTitle, displayTitle());
    if ( !self )
    {
        return false;
    }
    if ( !self->authoringSnapshotStillCurrent(revision, beforePrompt) )
    {
        return false;
    }
    if ( result == QMessageBox::Save )
    {
        const bool saved = self->saveDocument(false);
        return self && saved;
    }
    if ( result == QMessageBox::Discard && discardedAny )
    {
        *discardedAny = true;
    }
    return result == QMessageBox::Discard;
}

QString ResourceAuthoringDialog::fileFilter() const
{
    switch ( kind_ )
    {
        case ResourceAuthoringKind::sprite:
            return QStringLiteral("Sprite resources (*.spr);;All files (*)");
        case ResourceAuthoringKind::soundClass:
            return QStringLiteral("Sound classes (*.snd);;All files (*)");
        case ResourceAuthoringKind::stringTable:
            return QStringLiteral("String tables (*.uni);;All files (*)");
    }
    return QStringLiteral("All files (*)");
}

QString ResourceAuthoringDialog::extension() const
{
    switch ( kind_ )
    {
        case ResourceAuthoringKind::sprite: return QStringLiteral("spr");
        case ResourceAuthoringKind::soundClass: return QStringLiteral("snd");
        case ResourceAuthoringKind::stringTable: return QStringLiteral("uni");
    }
    return {};
}

QString ResourceAuthoringDialog::suggestedBaseName() const
{
    switch ( kind_ )
    {
        case ResourceAuthoringKind::sprite: return QStringLiteral("new_sprite.spr");
        case ResourceAuthoringKind::soundClass: return QStringLiteral("new_sound_class.snd");
        case ResourceAuthoringKind::stringTable:
            return context_.projectType == ProjectType::IJIM
                ? QStringLiteral("jonesstrings.uni")
                : QStringLiteral("cogstrings.uni");
    }
    return {};
}

std::filesystem::path ResourceAuthoringDialog::defaultDocumentPath() const
{
    if ( !context_.projectDirectory.empty() )
    {
        if ( kind_ == ResourceAuthoringKind::sprite )
        {
            return context_.projectDirectory / "misc" / "spr" / "new_sprite.spr";
        }
        if ( kind_ == ResourceAuthoringKind::soundClass )
        {
            return context_.projectDirectory / "misc" / "snd" / "new_sound_class.snd";
        }
        return context_.projectDirectory / "misc"
            / (context_.projectType == ProjectType::IJIM
                    ? "jonesstrings.uni"
                    : "cogstrings.uni");
    }
    const std::filesystem::path directory = context_.startingDirectory.empty()
        ? std::filesystem::path(QDir::homePath().toStdWString())
        : context_.startingDirectory;
    return directory / suggestedBaseName().toStdWString();
}

std::filesystem::path ResourceAuthoringDialog::openStartingPath() const
{
    if ( !currentPath_.empty() )
    {
        return currentPath_;
    }
    return defaultDocumentPath();
}

std::optional<std::filesystem::path> ResourceAuthoringDialog::choosePath(const bool saveAs)
{
    QPointer<ResourceAuthoringDialog> self(this);
    QFileDialog chooser(
        nullptr,
        saveAs
            ? QStringLiteral("Save Resource As")
            : QStringLiteral("Open Resource"),
        qPath(saveAs ? defaultDocumentPath() : openStartingPath()),
        fileFilter());
    chooser.setWindowModality(Qt::ApplicationModal);
    QObject::connect(this, &QObject::destroyed, &chooser, &QDialog::reject);
    chooser.setObjectName(QStringLiteral("resource_authoring.file_dialog"));
    chooser.setOption(QFileDialog::DontConfirmOverwrite, false);
    chooser.setDefaultSuffix(extension());
    if ( saveAs )
    {
        chooser.setAcceptMode(QFileDialog::AcceptSave);
        chooser.setFileMode(QFileDialog::AnyFile);
    }
    else
    {
        chooser.setAcceptMode(QFileDialog::AcceptOpen);
        chooser.setFileMode(QFileDialog::ExistingFile);
    }
    const std::filesystem::path seed = saveAs
        ? defaultDocumentPath()
        : openStartingPath();
    if ( !seed.empty() )
    {
        chooser.selectFile(qPath(seed));
    }
    const int dialogResult = chooser.exec();
    if ( !self
        || dialogResult != QDialog::Accepted
        || chooser.selectedFiles().isEmpty() )
    {
        return std::nullopt;
    }
    std::filesystem::path result(chooser.selectedFiles().front().toStdWString());
    if ( saveAs && result.extension().empty() )
    {
        result += "." + extension().toStdString();
    }
    return result;
}

void ResourceAuthoringDialog::openDocument(const bool skipReplaceConfirmation)
{
    lastOpenSucceeded_ = false;
    QPointer<ResourceAuthoringDialog> self(this);
    const std::optional<std::filesystem::path> selected = choosePath(false);
    if ( !self || !selected )
    {
        return;
    }
    const auto initialProjectContextStillCurrent = self->context_.projectContextStillCurrent;
    const bool initialProjectIsCurrent = !initialProjectContextStillCurrent
        || initialProjectContextStillCurrent();
    if ( !self )
    {
        return;
    }
    if ( !initialProjectIsCurrent )
    {
        self->setStatus(QStringLiteral("The active project changed; open the resource again."), true);
        return;
    }
    const auto activateExistingSource = self->context_.activateExistingSource;
    const bool alreadyOpen = activateExistingSource
        && activateExistingSource(self.data(), normalizedLooseIdentity(*selected));
    if ( !self )
    {
        return;
    }
    if ( alreadyOpen )
    {
        lastOpenSucceeded_ = true;
        return;
    }

    try
    {
        const std::optional<std::string> sourceBytes = readFileBytes(*selected);
        if ( !sourceBytes )
        {
            throw std::runtime_error("Could not read the selected resource.");
        }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        if ( self->afterReadHook_ )
        {
            ResourceAuthoringPaneAfterReadHook hook =
                std::move(self->afterReadHook_);
            self->afterReadHook_ = {};
            hook(*selected);
            if ( !self )
            {
                return;
            }
        }
#endif
        assets::SprFile loadedSprite{};
        assets::SoundClassFile loadedSound{};
        assets::StringTableFile loadedStrings{};
        std::istringstream input(*sourceBytes, std::ios::binary);
        if ( kind_ == ResourceAuthoringKind::sprite )
        {
            loadedSprite = assets::loadFromSPR(input);
        }
        else if ( kind_ == ResourceAuthoringKind::soundClass )
        {
            loadedSound = assets::loadFromSoundClass(input, soundProfile(context_.projectType));
        }
        else
        {
            loadedStrings = assets::loadFromStringTableUni(
                input,
                stringProfile_,
                qPath(*selected).toStdString());
        }

        if ( (!skipReplaceConfirmation && !confirmReplaceDocument()) || !self )
        {
            return;
        }
        if ( self->context_.projectContextStillCurrent
            && !self->context_.projectContextStillCurrent() )
        {
            self->setStatus(
                QStringLiteral("The active project changed; open the resource again."),
                true);
            return;
        }
        self->cancelDeferredEditorPresentation();
        self->soundManualColumnWidths_ = false;
        if ( self->kind_ == ResourceAuthoringKind::sprite )
        {
            self->spritePendingNumericEdits_.clear();
            self->spriteNumericEditBaselines_.clear();
            self->spriteNumericUndoKeys_.clear();
            self->sprite_ = std::move(loadedSprite);
            self->spriteProfile_ = defaultSpriteProfile(self->context_.projectType);
        }
        else if ( self->kind_ == ResourceAuthoringKind::soundClass )
        {
            self->soundFile_ = std::move(loadedSound);
            self->soundRawRows_.clear();
            self->soundPreviewLoopRows_.clear();
            self->knownSoundModes_.clear();
            self->soundPresentationModes_.clear();
            self->collapsedSoundModes_.clear();
        }
        else
        {
            self->stringsFile_ = std::move(loadedStrings);
        }
        self->currentPath_ = *selected;
        self->draftName_ = QString::fromStdWString(self->currentPath_.filename().wstring());
        self->projectResourceName_.clear();
        self->archiveSourceTooltip_.clear();
        self->sourceKind_ = SourceKind::loose;
        self->draftIdentity_.clear();
        self->diskBaselineHash_ = sha256(*sourceBytes);
        self->dirty_ = false;
        self->resetSoundHistory();
        ++self->authoredRevision_;
        self->successfulOpenNotification_ = true;
        if ( skipReplaceConfirmation )
        {
            self->context_.transientProjectOpenTarget = false;
        }
        self->setStatus(QStringLiteral("Opened."));
        self->refreshDocumentWidgets();
        if ( self )
        {
            self->lastOpenSucceeded_ = true;
            self->notifyChanged();
        }
    }
    catch (const std::exception& exception)
    {
        reportFailure(
            QStringLiteral("Could not open resource"),
            QString::fromUtf8(exception.what()));
    }
}

bool ResourceAuthoringDialog::openLooseFileForCaller(const bool skipReplaceConfirmation)
{
    QPointer<ResourceAuthoringDialog> self(this);
    if ( !self )
    {
        return false;
    }
    self->lastOpenSucceeded_ = false;
    self->openDocument(skipReplaceConfirmation);
    return self && self->lastOpenSucceeded_;
}

void ResourceAuthoringDialog::openProjectResource(const bool bypassWorkspaceCallback)
{
    lastProjectOpenSucceeded_ = false;
    QPointer<ResourceAuthoringDialog> self(this);
    if ( !bypassWorkspaceCallback && embedded_ && context_.openProjectResourceInNewTab )
    {
        const auto callback = context_.openProjectResourceInNewTab;
        const bool handled = callback(this, kind_);
        if ( self )
        {
            self->lastProjectOpenSucceeded_ = handled;
        }
        return;
    }
    if ( !context_.resources )
    {
        setStatus(QStringLiteral("No project resource locator is available."), true);
        return;
    }
    ResourcePickerOptions options{};
    switch ( kind_ )
    {
        case ResourceAuthoringKind::sprite: options.kind = ResourcePickerKind::spr; break;
        case ResourceAuthoringKind::soundClass: options.kind = ResourcePickerKind::snd; break;
        case ResourceAuthoringKind::stringTable: options.kind = ResourcePickerKind::uni; break;
    }
    options.projectDirectory = context_.projectDirectory.empty()
        ? std::nullopt
        : std::optional<std::filesystem::path>(context_.projectDirectory);
    options.preferredDiskRoot = options.projectDirectory;
    options.previewCmp = context_.masterCmp;
    options.previewProjectType = context_.projectType;
    const std::shared_ptr<const assets::ResourceLocator> locatorSnapshot = context_.resources;
    const std::optional<std::string> selected = pickResource(nullptr, locatorSnapshot, options);
    if ( !self || !selected )
    {
        return;
    }
    const QString selectedName = QString::fromLatin1(
        selected->data(), static_cast<qsizetype>(selected->size()));
    const auto resolvedProjectContextStillCurrent = self->context_.projectContextStillCurrent;
    const bool resolvedProjectIsCurrent = !resolvedProjectContextStillCurrent
        || resolvedProjectContextStillCurrent();
    if ( !self )
    {
        return;
    }
    if ( !resolvedProjectIsCurrent )
    {
        self->setStatus(QStringLiteral("The active project changed; open the resource again."), true);
        return;
    }

    std::filesystem::path looseSource{};
    std::optional<std::vector<std::byte>> bytes{};
    try
    {
        bytes = locatorSnapshot->tryReadBinary(
            *selected,
            kMaximumResourceAuthoringFileBytes,
            &looseSource);
    }
    catch ( const std::exception& exception )
    {
        if ( self )
        {
            self->reportFailure(
                QStringLiteral("Could not open project resource"),
                QString::fromUtf8(exception.what()));
        }
        return;
    }
    if ( !self )
    {
        return;
    }
    if ( !bytes )
    {
        self->reportFailure(
            QStringLiteral("Could not open project resource"),
            QStringLiteral("The selected resource is missing or exceeds the 16 MiB byte budget: %1")
                .arg(selectedName));
        return;
    }

    const bool isLooseSource = !looseSource.empty();
    const QString archiveSourceTooltip = isLooseSource
        ? QString{}
        : resolveArchiveSourceTooltip(locatorSnapshot.get(), *selected).tooltip;
    const QString resolvedIdentity = isLooseSource
        ? normalizedLooseIdentity(looseSource)
        : QStringLiteral("project:") + selectedName.trimmed().toCaseFolded();
    const auto finalProjectContextStillCurrent = self->context_.projectContextStillCurrent;
    const bool finalProjectIsCurrent = !finalProjectContextStillCurrent
        || finalProjectContextStillCurrent();
    if ( !self )
    {
        return;
    }
    if ( !finalProjectIsCurrent )
    {
        self->setStatus(QStringLiteral("The active project changed; open the resource again."), true);
        return;
    }
    const auto activateExistingSource = self->context_.activateExistingSource;
    const bool alreadyOpen = activateExistingSource
        && activateExistingSource(self.data(), resolvedIdentity);
    if ( !self )
    {
        return;
    }
    if ( alreadyOpen )
    {
        self->lastProjectOpenSucceeded_ = true;
        return;
    }

    assets::SprFile loadedSprite{};
    assets::SoundClassFile loadedSound{};
    assets::StringTableFile loadedStrings{};
    try
    {
        const std::string textBytes(reinterpret_cast<const char*>(bytes->data()), bytes->size());
        std::istringstream input(textBytes, std::ios::binary);
        if ( kind_ == ResourceAuthoringKind::sprite )
        {
            loadedSprite = assets::loadFromSPR(input);
        }
        else if ( kind_ == ResourceAuthoringKind::soundClass )
        {
            loadedSound = assets::loadFromSoundClass(input, soundProfile(context_.projectType));
        }
        else
        {
            loadedStrings = assets::loadFromStringTableUni(
                input,
                stringProfile_,
                toUtf8(selectedName));
        }
    }
    catch ( const std::exception& exception )
    {
        reportFailure(
            QStringLiteral("Could not open project resource"),
            QString::fromUtf8(exception.what()));
        return;
    }

    if ( (!context_.transientProjectOpenTarget && !confirmReplaceDocument()) || !self )
    {
        return;
    }
    const auto confirmationProjectContextStillCurrent =
        self->context_.projectContextStillCurrent;
    const bool confirmationProjectIsCurrent = !confirmationProjectContextStillCurrent
        || confirmationProjectContextStillCurrent();
    if ( !self )
    {
        return;
    }
    if ( !confirmationProjectIsCurrent )
    {
        self->setStatus(
            QStringLiteral("The active project changed; open the resource again."),
            true);
        return;
    }
    self->cancelDeferredEditorPresentation();
    self->soundManualColumnWidths_ = false;
    if ( self->kind_ == ResourceAuthoringKind::sprite )
    {
        self->spritePendingNumericEdits_.clear();
        self->spriteNumericEditBaselines_.clear();
        self->spriteNumericUndoKeys_.clear();
        self->sprite_ = std::move(loadedSprite);
        self->spriteProfile_ = defaultSpriteProfile(self->context_.projectType);
    }
    else if ( self->kind_ == ResourceAuthoringKind::soundClass )
    {
        self->soundFile_ = std::move(loadedSound);
        self->soundRawRows_.clear();
        self->soundPreviewLoopRows_.clear();
        self->knownSoundModes_.clear();
        self->soundPresentationModes_.clear();
        self->collapsedSoundModes_.clear();
    }
    else
    {
        self->stringsFile_ = std::move(loadedStrings);
    }
    self->currentPath_ = isLooseSource ? looseSource : std::filesystem::path{};
    self->sourceKind_ = isLooseSource ? SourceKind::loose : SourceKind::projectResource;
    self->projectResourceName_ = isLooseSource ? QString{} : selectedName;
    self->archiveSourceTooltip_ = isLooseSource ? QString{} : archiveSourceTooltip;
    self->draftName_ = QString::fromLatin1(
        std::filesystem::path(*selected).filename().string().c_str());
    self->draftIdentity_.clear();
    const std::string sourceBytes(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    self->diskBaselineHash_ = sha256(sourceBytes);
    self->dirty_ = false;
    self->resetSoundHistory();
    ++self->authoredRevision_;
    self->successfulOpenNotification_ = true;
    self->context_.transientProjectOpenTarget = false;
    self->setStatus(QStringLiteral("Opened."));
    self->refreshDocumentWidgets();
    if ( self )
    {
        self->lastProjectOpenSucceeded_ = true;
        self->notifyChanged();
    }
}

bool ResourceAuthoringDialog::openProjectResourceForCaller(const bool bypassWorkspaceCallback)
{
    QPointer<ResourceAuthoringDialog> self(this);
    if ( !self )
    {
        return false;
    }
    self->lastProjectOpenSucceeded_ = false;
    self->openProjectResource(bypassWorkspaceCallback);
    return self && self->lastProjectOpenSucceeded_;
}

std::string ResourceAuthoringDialog::serializeCurrentDocument(
    std::vector<SoundHistoryEdit>* const soundEditsAfterSave) const
{
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        assets::SprFile output = sprite_;
        commitPendingSpriteNumericEdits(output);
        const auto material = ui::episode::gameBytesForDisplayText(spriteMaterial_->text());
        if ( !material )
        {
            throw std::runtime_error(
                "Sprite material text must be representable in the game's single-byte encoding.");
        }
        output.material = *material;
        return assets::saveToSPRText(output, spriteProfile_);
    }
    if ( kind_ == ResourceAuthoringKind::soundClass )
    {
        assets::SoundClassFile output = soundFile_;
        if ( !soundEntries_ )
        {
            throw std::runtime_error("The sound-class tree is unavailable.");
        }
        for ( std::size_t index = 0; index < output.entries.size(); ++index )
        {
            const QTreeWidgetItem* const item = soundAlternativeAt(static_cast<int>(index));
            if ( !item )
            {
                throw std::runtime_error("Sound-class rows are out of sync with the document.");
            }
            assets::SoundClassEntry& entry = output.entries[index];
            const QByteArray mode = item->data(0, detail::kSoundTreeModeBytesRole).toByteArray();
            const std::optional<std::string> wav =
                ui::episode::gameBytesForDisplayText(item->text(1));
            const std::optional<std::uint32_t> flags = parseHexFlags(item->text(2));
            const assets::SoundClassEntry& accepted = soundFile_.entries[index];
            std::array<QString, 3> numericText{};
            const auto parseNumeric = [](const QString& raw, const float acceptedValue, QString& canonical)
                -> std::optional<float>
            {
                canonical = raw;
                // Accepted raw rows are canonical engine tokens. A restored
                // unfinished row has no widget locale snapshot; parse it only
                // in the current UI locale, without guessing another separator.
                if ( const auto value = parseFiniteFloat(raw);
                    value && std::bit_cast<std::uint32_t>(*value) == std::bit_cast<std::uint32_t>(acceptedValue) )
                {
                    return value;
                }
                canonical = ui::canonicalUiNumericText(raw, ui::uiNumericLocale());
                return parseFiniteFloat(canonical);
            };
            const std::optional<float> minRadius = parseNumeric(item->text(3), accepted.minRadius, numericText[0]);
            const std::optional<float> maxRadius = parseNumeric(item->text(4), accepted.maxRadius, numericText[1]);
            const std::optional<float> maxVolume = parseNumeric(item->text(5), accepted.maxVolume, numericText[2]);
            if ( mode.isEmpty() || !wav || !flags || !minRadius || !maxRadius || !maxVolume )
            {
                throw std::runtime_error(
                    "Sound-class row " + std::to_string(index + 1)
                    + " contains text outside the game's single-byte encoding, an invalid flag, or a non-finite number.");
            }
            entry.mode.assign(mode.constData(), static_cast<std::size_t>(mode.size()));
            entry.wav = *wav;
            entry.playFlags = *flags;
            entry.minRadius = *minRadius;
            entry.maxRadius = *maxRadius;
            entry.maxVolume = *maxVolume;
            if ( soundEditsAfterSave &&
                (numericText[0] != item->text(3) || numericText[1] != item->text(4) || numericText[2] != item->text(5)
                 || std::bit_cast<std::uint32_t>(entry.minRadius) != std::bit_cast<std::uint32_t>(accepted.minRadius)
                 || std::bit_cast<std::uint32_t>(entry.maxRadius) != std::bit_cast<std::uint32_t>(accepted.maxRadius)
                 || std::bit_cast<std::uint32_t>(entry.maxVolume) != std::bit_cast<std::uint32_t>(accepted.maxVolume)) )
            {
                const SoundRowRecord before = soundRowRecord(static_cast<int>(index));
                SoundRowRecord after = before;
                after.entry = entry;
                for ( std::size_t field = 0; field < numericText.size(); ++field )
                {
                    after.raw[field + 2] = std::move(numericText[field]);
                }
                soundEditsAfterSave->push_back(SoundHistoryEdit{
                    static_cast<int>(index), before, std::move(after)});
            }
        }
        return assets::saveToSoundClassText(output, soundProfile(context_.projectType));
    }

    assets::StringTableFile output = stringsFile_;
    if ( !stringTable_ || stringTable_->rowCount() != static_cast<int>(stringsFile_.entries.size()) )
    {
        throw std::runtime_error("String-table row table no longer matches the document.");
    }
    output.entries.clear();
    output.entries.reserve(static_cast<std::size_t>(stringTable_->rowCount()));
    for ( int row = 0; row < stringTable_->rowCount(); ++row )
    {
        const auto cellText = [this, row](const int column)
        {
            const QTableWidgetItem* const item = stringTable_->item(row, column);
            if ( !item )
            {
                throw std::runtime_error("A string-table row has a missing field.");
            }
            return item->text();
        };
        const std::optional<std::string> key =
            ui::episode::gameBytesForDisplayText(cellText(0));
        const std::optional<std::string> value =
            ui::episode::gameBytesForDisplayText(cellText(1));
        bool numberOk = false;
        const qlonglong parsedNumber = cellText(2).trimmed().toLongLong(&numberOk, 10);
        if ( !key || !value || !numberOk )
        {
            throw std::runtime_error(
                "String-table row " + std::to_string(row + 1)
                + " must use game-byte text and a signed decimal legacy number.");
        }
        if ( parsedNumber < std::numeric_limits<std::int32_t>::min()
            || parsedNumber > std::numeric_limits<std::int32_t>::max() )
        {
            throw std::runtime_error(
                "String-table row " + std::to_string(row + 1)
                + " has a number outside the signed 32-bit range.");
        }
        output.entries.push_back(assets::StringTableEntry{
            *key, static_cast<std::int32_t>(parsedNumber), *value });
    }
    if ( stringProfile_ == assets::UniProfile::classic )
    {
        output.capacity = std::max(
            output.capacity,
            static_cast<std::uint32_t>(output.entries.size()));
    }
    return assets::saveToStringTableUniText(output, stringProfile_);
}

std::optional<float> ResourceAuthoringDialog::spriteNumericValue(const QString& key) const
{
    if ( key == QStringLiteral("width") )
    {
        return sprite_.width;
    }
    if ( key == QStringLiteral("height") )
    {
        return sprite_.height;
    }
    if ( key == QStringLiteral("extraLightScalar") )
    {
        return sprite_.extraLightRGBA[0];
    }
    if ( key == QStringLiteral("offsetX") )
    {
        return sprite_.offsetXYZ[0];
    }
    if ( key == QStringLiteral("offsetY") )
    {
        return sprite_.offsetXYZ[1];
    }
    if ( key == QStringLiteral("offsetZ") )
    {
        return sprite_.offsetXYZ[2];
    }
    return std::nullopt;
}

bool ResourceAuthoringDialog::setSpritePendingNumericEdit(const QString& key, const QString& text)
{
    if ( kind_ != ResourceAuthoringKind::sprite || refreshing_ )
    {
        return false;
    }
    const std::optional<float> canonical = spriteNumericValue(key);
    auto* const line = qobject_cast<QLineEdit*>(sender());
    const std::optional<float> parsed = parseFiniteFloat(ui::canonicalUiNumericText(
        text, line ? line->locale() : ui::uiNumericLocale()));
    const bool sameCanonicalBits =
        parsed && canonical && std::bit_cast<std::uint32_t>(*parsed) == std::bit_cast<std::uint32_t>(*canonical);
    const bool nativeTextUndo = line && (line->property("resource_authoring.sprite_undo_request").toBool() ||
                                         (line->isRedoAvailable() && !line->isUndoAvailable()));
    if ( line )
    {
        line->setProperty("resource_authoring.sprite_undo_request", false);
    }
    if ( !nativeTextUndo )
    {
        // Navigation leaves Undo suppression intact; a fresh text mutation
        // (including a context-menu paste or cut) starts a new authored edit.
        spriteNumericUndoKeys_.remove(key);
    }
    const bool undoneToOriginalDisplay =
        nativeTextUndo && spriteNumericEditBaselines_.contains(key) && spriteNumericEditBaselines_.value(key) == text;
    if ( sameCanonicalBits || undoneToOriginalDisplay )
    {
        const bool changed = clearSpritePendingNumericEdit(key);
        if ( undoneToOriginalDisplay && !sameCanonicalBits )
        {
            spriteNumericUndoKeys_.insert(key);
        }
        return changed;
    }
    if ( spritePendingNumericEdits_.value(key).toString() == text && spritePendingNumericEdits_.contains(key) )
    {
        return false;
    }
    spritePendingNumericEdits_.insert(key, text);
    spriteHistoryPendingKey_ = key;
    return true;
}

bool ResourceAuthoringDialog::clearSpritePendingNumericEdit(const QString& key)
{
    if ( kind_ != ResourceAuthoringKind::sprite || refreshing_ )
    {
        return false;
    }
    spriteNumericEditBaselines_.remove(key);
    spriteNumericUndoKeys_.remove(key);
    if ( !spritePendingNumericEdits_.contains(key) )
    {
        return false;
    }
    spritePendingNumericEdits_.remove(key);
    return true;
}

void ResourceAuthoringDialog::restorePendingSpriteNumericEditors()
{
    const auto restore = [this](const QString& key, ui::ScrubFloatEdit* control)
    {
        if ( control && spritePendingNumericEdits_.contains(key) )
        {
            const QSignalBlocker blocker(control->lineEdit());
            control->lineEdit()->setText(spritePendingNumericEdits_.value(key).toString());
        }
    };
    restore(QStringLiteral("width"), spriteWidth_);
    restore(QStringLiteral("height"), spriteHeight_);
    restore(QStringLiteral("extraLightScalar"), spriteExtraScalar_);
    restore(QStringLiteral("offsetX"), spriteOffset_[0]);
    restore(QStringLiteral("offsetY"), spriteOffset_[1]);
    restore(QStringLiteral("offsetZ"), spriteOffset_[2]);
}

void ResourceAuthoringDialog::commitPendingSpriteNumericEdits(assets::SprFile& output) const
{
    const auto commit = [this](const QString& key, float& value, const bool positive,
        const ui::ScrubFloatEdit* const control)
    {
        if ( !spritePendingNumericEdits_.contains(key) )
        {
            return;
        }
        const QString raw = spritePendingNumericEdits_.value(key).toString();
        const std::optional<float> parsed = parseFiniteFloat(ui::canonicalUiNumericText(
            raw, control ? control->lineEdit()->locale() : ui::uiNumericLocale()));
        if ( !parsed )
        {
            throw std::runtime_error(
                QStringLiteral("Sprite %1 must contain a finite number.").arg(key).toStdString());
        }
        if ( positive && !(*parsed > 0.0f) )
        {
            throw std::runtime_error(
                QStringLiteral("Sprite %1 must be greater than zero.").arg(key).toStdString());
        }
        value = *parsed;
    };
    commit(QStringLiteral("width"), output.width, true, spriteWidth_);
    commit(QStringLiteral("height"), output.height, true, spriteHeight_);
    commit(QStringLiteral("extraLightScalar"), output.extraLightRGBA[0], false, spriteExtraScalar_);
    if ( spriteProfile_ == assets::SprProfile::classic &&
         spritePendingNumericEdits_.contains(QStringLiteral("extraLightScalar")) )
    {
        output.extraLightRGBA[1] = output.extraLightRGBA[0];
        output.extraLightRGBA[2] = output.extraLightRGBA[0];
    }
    commit(QStringLiteral("offsetX"), output.offsetXYZ[0], false, spriteOffset_[0]);
    commit(QStringLiteral("offsetY"), output.offsetXYZ[1], false, spriteOffset_[1]);
    commit(QStringLiteral("offsetZ"), output.offsetXYZ[2], false, spriteOffset_[2]);
    if ( !(output.width > 0.0f) || !(output.height > 0.0f)
        || !std::isfinite(output.width) || !std::isfinite(output.height) )
    {
        throw std::runtime_error("Sprite width and height must be greater than zero.");
    }
}

void ResourceAuthoringDialog::commitPendingSpriteNumericEditsAfterSave()
{
    if ( spritePendingNumericEdits_.isEmpty() )
    {
        return;
    }
    assets::SprFile output = sprite_;
    commitPendingSpriteNumericEdits(output);
    const bool wasRefreshing = refreshing_;
    refreshing_ = true;
    sprite_.width = output.width;
    sprite_.height = output.height;
    sprite_.extraLightRGBA = output.extraLightRGBA;
    sprite_.offsetXYZ = output.offsetXYZ;
    if ( spriteWidth_ )
    {
        spriteWidth_->setValue(sprite_.width);
    }
    if ( spriteHeight_ )
    {
        spriteHeight_->setValue(sprite_.height);
    }
    if ( spriteExtraScalar_ )
    {
        spriteExtraScalar_->setValue(sprite_.extraLightRGBA[0]);
    }
    for ( std::size_t index = 0; index < spriteOffset_.size(); ++index )
    {
        if ( spriteOffset_[index] )
        {
            spriteOffset_[index]->setValue(sprite_.offsetXYZ[index]);
        }
    }
    spritePendingNumericEdits_.clear();
    spriteNumericEditBaselines_.clear();
    spriteNumericUndoKeys_.clear();
    refreshing_ = wasRefreshing;
    scheduleSpritePreview();
}

bool ResourceAuthoringDialog::saveDocument(const bool saveAs)
{
    QPointer<ResourceAuthoringDialog> self(this);
    settleActiveTableEditors();
    if ( !self )
    {
        return false;
    }
    const std::uint64_t authoredRevision = authoredRevision_;
    const QCborMap authoredState = authoredContentState();
    const auto stateStillCurrent = [&]()
    {
        return self
            && self->authoringSnapshotStillCurrent(authoredRevision, authoredState);
    };

    std::string serialized{};
    std::vector<SoundHistoryEdit> soundEditsAfterSave{};
    try
    {
        serialized = serializeCurrentDocument(&soundEditsAfterSave);
    }
    catch (const std::exception& exception)
    {
        reportFailure(QStringLiteral("Resource validation failed"), QString::fromUtf8(exception.what()));
        return false;
    }

    std::filesystem::path target = currentPath_;
    const bool automaticArchiveSave = !saveAs && sourceKind_ == SourceKind::projectResource;
    const bool requiresLooseDestination = sourceKind_ != SourceKind::loose || target.empty();
    if ( automaticArchiveSave )
    {
        std::filesystem::path directory = context_.automaticSaveDirectory;
        if ( directory.empty() && !context_.projectDirectory.empty() )
        {
            directory = context_.projectDirectory / "misc";
            if ( kind_ != ResourceAuthoringKind::stringTable )
            {
                directory /= kind_ == ResourceAuthoringKind::sprite ? "spr" : "snd";
            }
        }
        const std::filesystem::path leaf(projectResourceName_.toStdWString());
        if ( directory.empty() || leaf.filename().empty() || leaf.filename() == "." || leaf.filename() == ".." )
        {
            reportFailure(QStringLiteral("Could not save resource"),
                          QStringLiteral("No project resource directory is configured for this resource."));
            return false;
        }
        target = directory / leaf.filename();
    }
    else if ( saveAs || requiresLooseDestination )
    {
        const std::optional<std::filesystem::path> selected = choosePath(true);
        if ( !self || !selected || !stateStillCurrent() )
        {
            return false;
        }
        target = *selected;
    }
    else if ( !stateStillCurrent() )
    {
        return false;
    }

    bool canonicalTargetIsExistingLooseSource{};
    if ( !context_.projectDirectory.empty() )
    {
        const bool ordinaryExistingLooseSourceSave = !saveAs
            && sourceKind_ == SourceKind::loose
            && !currentPath_.empty();
        std::error_code error{};
        const std::filesystem::path project = std::filesystem::weakly_canonical(
            context_.projectDirectory, error);
        if ( error )
        {
            reportFailure(
                QStringLiteral("Could not save resource"),
                QStringLiteral("The active project directory could not be resolved."));
            return false;
        }
        const std::optional<std::filesystem::path> canonicalTarget = canonicalPathForWrite(target);
        const std::optional<std::filesystem::path> canonicalProject = canonicalPathForWrite(project);
        const std::optional<std::filesystem::path> canonicalCurrentSource =
            ordinaryExistingLooseSourceSave
                ? canonicalPathForWrite(currentPath_)
                : std::nullopt;
        if ( !canonicalTarget || !canonicalProject
            || (ordinaryExistingLooseSourceSave && !canonicalCurrentSource) )
        {
            reportFailure(
                QStringLiteral("Could not save resource"),
                QStringLiteral("The selected destination could not be resolved."));
            return false;
        }
        const QString normalizedProject = QDir::cleanPath(qPath(*canonicalProject)).toCaseFolded();
        const QString normalizedTarget = QDir::cleanPath(qPath(*canonicalTarget)).toCaseFolded();
        const QString projectPrefix = normalizedProject.endsWith(QLatin1Char('/'))
            ? normalizedProject
            : normalizedProject + QLatin1Char('/');
        const bool targetsExistingLooseSource = ordinaryExistingLooseSourceSave
            && canonicalCurrentSource
            && normalizedTarget == QDir::cleanPath(qPath(*canonicalCurrentSource)).toCaseFolded();
        canonicalTargetIsExistingLooseSource = targetsExistingLooseSource;
        if ( normalizedTarget != normalizedProject
            && !normalizedTarget.startsWith(projectPrefix)
            && !targetsExistingLooseSource )
        {
            reportFailure(
                QStringLiteral("Unsafe resource destination"),
                QStringLiteral("Project resource drafts must be saved as loose files inside the active project."));
            return false;
        }
        target = *canonicalTarget;
    }

    const auto normalizedPath = [](const std::filesystem::path& path)
    {
        std::error_code error{};
        const std::filesystem::path absolute = std::filesystem::absolute(path, error).lexically_normal();
        return QDir::cleanPath(qPath(error ? path.lexically_normal() : absolute)).toCaseFolded();
    };
    const QString targetIdentity = normalizedPath(target);
    const bool overwritesCurrentSource = !saveAs
        && sourceKind_ == SourceKind::loose
        && !currentPath_.empty()
        && (normalizedPath(currentPath_) == targetIdentity
            || canonicalTargetIsExistingLooseSource);
    const auto destinationAvailable = context_.destinationAvailable;
    const auto destinationIsAvailable = [&]()
    {
        if ( !self )
        {
            return false;
        }
        if ( destinationAvailable && !destinationAvailable(self.data(), target) )
        {
            if ( self )
            {
                self->setStatus(
                    QStringLiteral("Another open resource editor already owns this destination."),
                    true);
            }
            return false;
        }
        return stateStillCurrent();
    };
    if ( !destinationIsAvailable() )
    {
        return false;
    }

    if ( overwritesCurrentSource )
    {
        std::optional<std::string> currentBytes{};
        try
        {
            currentBytes = readFileBytes(currentPath_);
        }
        catch ( const std::exception& )
        {
            // Treat an unreadable or over-budget disk version as an external change.
        }
        const QByteArray currentHash = currentBytes ? sha256(*currentBytes) : QByteArray{};
        if ( currentHash != diskBaselineHash_ )
        {
            QMessageBox changedOnDisk(
                QMessageBox::Warning,
                QStringLiteral("Resource Changed on Disk"),
                QStringLiteral("This loose resource changed or disappeared since it was opened. Choose whether to reload it or overwrite the disk version."),
                QMessageBox::NoButton,
                nullptr);
            changedOnDisk.setWindowModality(Qt::ApplicationModal);
            QObject::connect(this, &QObject::destroyed, &changedOnDisk, &QDialog::reject);
            changedOnDisk.setObjectName(QStringLiteral("resource_authoring.external_change_prompt"));
            QPushButton* overwriteButton = changedOnDisk.addButton(
                QStringLiteral("Overwrite Disk"), QMessageBox::AcceptRole);
            QPushButton* reloadButton = changedOnDisk.addButton(
                QStringLiteral("Reload from Disk"), QMessageBox::DestructiveRole);
            QPushButton* cancelButton = changedOnDisk.addButton(
                QMessageBox::Cancel);
            changedOnDisk.setDefaultButton(cancelButton);
            changedOnDisk.setEscapeButton(cancelButton);
            changedOnDisk.exec();
            if ( !self || !stateStillCurrent() )
            {
                return false;
            }
            if ( changedOnDisk.clickedButton() == reloadButton )
            {
                (void)reloadCurrentLooseSource();
                return false;
            }
            if ( changedOnDisk.clickedButton() != overwriteButton )
            {
                return false;
            }
        }
    }

    bool destinationExists = false;
    std::error_code directoryError{};
    const std::filesystem::path parent = target.parent_path();
    for ( std::filesystem::directory_iterator it(parent, directoryError), end;
          !directoryError && it != end;
          it.increment(directoryError) )
    {
        if ( QDir::toNativeSeparators(QString::fromStdWString(it->path().filename().wstring()))
                .compare(QDir::toNativeSeparators(qPath(target.filename())), Qt::CaseInsensitive) == 0 )
        {
            destinationExists = true;
            break;
        }
    }
    std::error_code parentExistsError{};
    const bool parentExists = std::filesystem::exists(parent, parentExistsError);
    if ( directoryError && parentExists && !parentExistsError )
    {
        reportFailure(
            QStringLiteral("Could not save resource"),
            QStringLiteral("The destination directory could not be inspected."));
        return false;
    }
    if ( destinationExists && !overwritesCurrentSource )
    {
        QMessageBox overwrite(
            QMessageBox::Warning,
            QStringLiteral("Replace Resource?"),
            QStringLiteral("A file with this name already exists in the destination. Replace it?"),
            QMessageBox::Yes | QMessageBox::Cancel,
            nullptr);
        overwrite.setWindowModality(Qt::ApplicationModal);
        QObject::connect(this, &QObject::destroyed, &overwrite, &QDialog::reject);
        overwrite.setObjectName(QStringLiteral("resource_authoring.overwrite_prompt"));
        overwrite.setDefaultButton(QMessageBox::Cancel);
        overwrite.setEscapeButton(QMessageBox::Cancel);
        const int decision = overwrite.exec();
        if ( !self || !stateStillCurrent() || decision != QMessageBox::Yes )
        {
            return false;
        }
    }

    if ( !destinationIsAvailable() )
    {
        return false;
    }

    std::string error{};
    if ( automaticArchiveSave )
    {
        std::error_code directoryError{};
        std::filesystem::create_directories(target.parent_path(), directoryError);
        if ( directoryError )
        {
            reportFailure(QStringLiteral("Could not save resource"), QString::fromStdString(directoryError.message()));
            return false;
        }
    }
    if ( !ui::episode::writeTextFileAtomically(target, serialized, &error) )
    {
        reportFailure(
            QStringLiteral("Could not save resource"),
            QString::fromStdString(error));
        return false;
    }
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        commitPendingSpriteNumericEditsAfterSave();
        recordDocumentHistory();
    }
    else if ( kind_ == ResourceAuthoringKind::soundClass && !soundEditsAfterSave.empty() )
    {
        const QSignalBlocker blocker(soundEntries_);
        for ( SoundHistoryEdit& edit : soundEditsAfterSave )
        {
            const std::size_t row = static_cast<std::size_t>(edit.row);
            soundFile_.entries[row] = edit.after->entry;
            soundRawRows_[row] = edit.after->raw;
            if ( QTreeWidgetItem* const item = soundAlternativeAt(edit.row) )
            {
                for ( int column = 3; column <= 5; ++column )
                {
                    item->setText(column, edit.after->raw[static_cast<std::size_t>(column - 1)]);
                }
            }
            // A restored raw row may also have an unchanged live delegate.
            // Refresh only that editor's settled presentation after publication.
            for ( const LiveTableEditor& editor : liveTableEditors_ )
            {
                if ( editor.view.data() == soundEntries_ && editor.editor && editor.index.isValid()
                    && editor.index.column() >= 3 && editor.index.column() <= 5
                    && QModelIndex(editor.index).siblingAtColumn(0).data(kSoundTreeSourceIndexRole).toInt() == edit.row
                    && editor.view->indexWidget(editor.index) == editor.editor.data() )
                {
                    editor.view->itemDelegateForIndex(editor.index)->setEditorData(editor.editor.data(), editor.index);
                }
            }
            if ( edit.before && edit.after )
            {
                for ( int column = 3; column <= 5; ++column )
                {
                    const std::size_t rawIndex = static_cast<std::size_t>(column - 1);
                    if ( edit.before->raw[rawIndex] != edit.after->raw[rawIndex] )
                    {
                        pendingTableEditorCarets_.remove(
                            QStringLiteral("snd.entries/%1/%2").arg(edit.row).arg(column));
                    }
                }
            }
            recordSoundHistoryEdit(std::move(edit));
        }
    }
    currentPath_ = std::move(target);
    sourceKind_ = SourceKind::loose;
    draftName_ = QString::fromStdWString(currentPath_.filename().wstring());
    projectResourceName_.clear();
    archiveSourceTooltip_.clear();
    draftIdentity_.clear();
    diskBaselineHash_ = sha256(serialized);
    ui::acceptLooseDocumentSource(this, qPath(currentPath_), diskBaselineHash_);
    dirty_ = false;
    soundHistoryBaselineDirty_ = false;
    soundHistoryCleanCursor_ = soundHistoryCursor_;
    soundHistoryCleanAuthoredCursor_ = soundHistoryAuthoredCursor_;
    documentHistoryBaselineDirty_ = false;
    documentHistoryCleanCursor_ = documentHistoryCursor_;
    updateSoundHistoryActions();
    ++authoredRevision_;
    setStatus(QStringLiteral("Saved."));
    updateDocumentLabel();
    const auto briefNotice = context_.briefNotice;
    const QString savedMessage = QStringLiteral("Saved %1").arg(qPath(currentPath_));
    notifyChanged();
    if ( self && automaticArchiveSave && briefNotice )
    {
        briefNotice(self.data(), savedMessage);
    }
    return true;
}

bool ResourceAuthoringDialog::reloadCurrentLooseSource(QString* error)
{
    const auto failed = [this, error](const QString& detail)
    {
        if ( error )
        {
            *error = detail;
        }
        else
        {
            reportFailure(QStringLiteral("Could not reload resource"), detail);
        }
        return false;
    };
    if ( currentPath_.empty() )
    {
        return failed(QStringLiteral("The loose resource no longer exists or cannot be read."));
    }
    std::optional<std::string> bytes{};
    try
    {
        bytes = readFileBytes(currentPath_);
    }
    catch ( const std::exception& exception )
    {
        return failed(QString::fromUtf8(exception.what()));
    }
    if ( !bytes )
    {
        return failed(QStringLiteral("The loose resource no longer exists or cannot be read."));
    }
    assets::SprFile loadedSprite{};
    assets::SoundClassFile loadedSound{};
    assets::StringTableFile loadedStrings{};
    try
    {
        std::istringstream input(*bytes, std::ios::binary);
        if ( kind_ == ResourceAuthoringKind::sprite )
        {
            loadedSprite = assets::loadFromSPR(input);
        }
        else if ( kind_ == ResourceAuthoringKind::soundClass )
        {
            loadedSound = assets::loadFromSoundClass(input, soundProfile(context_.projectType));
        }
        else
        {
            loadedStrings = assets::loadFromStringTableUni(
                input, stringProfile_, qPath(currentPath_).toStdString());
        }
    }
    catch ( const std::exception& exception )
    {
        return failed(QString::fromUtf8(exception.what()));
    }
    cancelDeferredEditorPresentation();
    soundManualColumnWidths_ = false;
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        spritePendingNumericEdits_.clear();
        spriteNumericEditBaselines_.clear();
        spriteNumericUndoKeys_.clear();
        sprite_ = std::move(loadedSprite);
        spriteProfile_ = defaultSpriteProfile(context_.projectType);
    }
    else if ( kind_ == ResourceAuthoringKind::soundClass )
    {
        soundFile_ = std::move(loadedSound);
        soundRawRows_.clear();
        soundPreviewLoopRows_.clear();
        knownSoundModes_.clear();
        soundPresentationModes_.clear();
        collapsedSoundModes_.clear();
    }
    else
    {
        stringsFile_ = std::move(loadedStrings);
    }
    diskBaselineHash_ = sha256(*bytes);
    ui::acceptLooseDocumentSource(this, qPath(currentPath_), diskBaselineHash_);
    dirty_ = false;
    resetSoundHistory();
    ++authoredRevision_;
    setStatus(QStringLiteral("Reloaded."));
    refreshDocumentWidgets();
    notifyChanged();
    return true;
}

void ResourceAuthoringDialog::markDirty(const bool preserveStatus)
{
    if ( refreshing_ )
    {
        return;
    }
    ++authoredRevision_;
    recordDocumentHistory();
    dirty_ = kind_ == ResourceAuthoringKind::soundClass
        ? soundHistoryBaselineDirty_ || soundHistoryAuthoredCursor_ != soundHistoryCleanAuthoredCursor_ || hasPendingDocumentEdit()
        : documentHistoryBaselineDirty_ || documentHistoryCursor_ != documentHistoryCleanCursor_ || hasPendingDocumentEdit();
    if ( !preserveStatus )
    {
        setStatus(dirty_ ? QStringLiteral("Unsaved changes.") : QStringLiteral("Changes reverted."));
    }
    updateDocumentLabel();
    notifyChanged();
}

void ResourceAuthoringDialog::updateDocumentLabel()
{
    const QString sourceTooltip = sourceKind_ == SourceKind::loose
        ? qPath(currentPath_)
        : sourceKind_ == SourceKind::projectResource && !archiveSourceTooltip_.isEmpty()
            ? archiveSourceTooltip_
            : projectResourceName_;
    setProperty("resource_authoring.archiveTooltip",
        sourceKind_ == SourceKind::projectResource ? archiveSourceTooltip_ : QString{});
    setProperty("resource_authoring.sourceTooltip", sourceTooltip);
    if ( sourceKind_ != SourceKind::loose || currentPath_.empty() )
    {
        ui::stopLooseDocumentWatch(this);
        return;
    }
    const QString path = qPath(currentPath_);
    if ( ui::looseDocumentWatchMatches(this, path, diskBaselineHash_) )
    {
        return;
    }
    const QPointer<ResourceAuthoringDialog> self(this);
    const auto projectStillCurrent = context_.projectContextStillCurrent;
    ui::LooseDocumentWatchOptions watch{};
    watch.path = path;
    watch.acceptedFingerprint = diskBaselineHash_;
    watch.maximumFileBytes = 16 * 1024 * 1024;
    watch.stillCurrent = [self, projectStillCurrent]()
    {
        return self && self->sourceKind_ == SourceKind::loose
            && !self->property("resource_authoring.force_close").toBool()
            && (!projectStillCurrent || projectStillCurrent());
    };
    watch.authoredRevision = [self]() { return self ? self->authoredRevision_ : 0; };
    watch.reload = [self]()
    {
        if ( !self )
        {
            return ui::LooseDocumentReloadResult{};
        }
        QString error{};
        const bool loaded = self->reloadCurrentLooseSource(&error);
        // A terminal Changed callback may have destroyed the pane.
        return self ? ui::LooseDocumentReloadResult{loaded, self->diskBaselineHash_, error}
                    : ui::LooseDocumentReloadResult{};
    };
    ui::watchLooseDocument(this, std::move(watch));
}

void ResourceAuthoringDialog::notifyChanged()
{
    const ResourceAuthoringChangedCallback callback = changed_;
    if ( callback )
    {
        callback(this);
    }
}

void ResourceAuthoringDialog::setStatus(const QString& text, const bool error)
{
    if ( !statusLabel_ )
    {
        return;
    }
    statusLabel_->setText(text);
    statusLabel_->setProperty("error", error);
    statusLabel_->setStyleSheet(error
            ? QStringLiteral("color: #c0392b")
            : QStringLiteral("color: palette(text)"));
}

void ResourceAuthoringDialog::reportFailure(const QString& title, const QString& detail)
{
    const QPointer<ResourceAuthoringDialog> self(this);
    const std::uint64_t authoredRevision = authoredRevision_;
    const QString failureStatus = QStringLiteral("%1: %2").arg(title, detail);
    setStatus(failureStatus, true);
    QMessageBox message(QMessageBox::Critical, title, detail, QMessageBox::Ok, nullptr);
    message.setWindowModality(Qt::ApplicationModal);
    QObject::connect(this, &QObject::destroyed, &message, &QDialog::reject);
    message.setObjectName(QStringLiteral("resource_authoring.error"));
    message.exec();
    if ( self && self->authoredRevision_ == authoredRevision )
    {
        if ( self->kind_ == ResourceAuthoringKind::sprite && self->previewTimer_->isActive() )
        {
            self->previewTimer_->stop();
            self->rebuildSpritePreview();
        }
        if ( self && self->authoredRevision_ == authoredRevision )
        {
            self->setStatus(failureStatus, true);
        }
    }
}

} // namespace detail

QDialog* createResourceAuthoringEditor(
    QWidget* const parent,
    const ResourceAuthoringKind kind,
    ResourceAuthoringContext context)
{
    return new detail::ResourceAuthoringDialog(parent, kind, std::move(context));
}

QWidget* createResourceAuthoringPane(
    QWidget* const parent,
    const ResourceAuthoringKind kind,
    ResourceAuthoringContext context,
    ResourceAuthoringChangedCallback changed)
{
    return new detail::ResourceAuthoringDialog(
        parent, kind, std::move(context), true, std::move(changed));
}

QCborMap captureResourceAuthoringPaneState(const QWidget* pane, const bool includeHistory)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor ? editor->captureState(includeHistory) : QCborMap{};
}

bool validateResourceAuthoringPaneState(const QCborMap& state, QString* warning)
{
    return detail::ResourceAuthoringDialog::validateState(state, warning);
}

bool restoreResourceAuthoringPaneState(
    QWidget* pane,
    const QCborMap& state,
    QString* warning)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    if ( !editor )
    {
        if ( warning )
        {
            *warning = QStringLiteral("The resource editor page is unavailable.");
        }
        return false;
    }
    if ( state.value(QStringLiteral("kind")).toInteger()
        != static_cast<int>(editor->kind()) )
    {
        if ( warning )
        {
            *warning = QStringLiteral("The resource editor kind does not match its recovery record.");
        }
        return false;
    }
    return editor->restoreState(state, warning);
}

ResourceAuthoringKind resourceAuthoringPaneKind(const QWidget* pane)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor ? editor->kind() : ResourceAuthoringKind::sprite;
}

QString resourceAuthoringPaneIdentity(const QWidget* pane)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor ? editor->identity() : QString{};
}

QString resourceAuthoringPaneTitle(const QWidget* pane)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor ? editor->displayTitle() : QString{};
}

QString resourceAuthoringPaneLooseFilePath(const QWidget* pane)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor ? editor->looseFilePath() : QString{};
}

bool resourceAuthoringPaneTakeSuccessfulOpenNotification(QWidget* pane)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor ? editor->takeSuccessfulOpenNotification() : false;
}

void resourceAuthoringPaneRebindProjectDirectory(
    QWidget* pane,
    const std::filesystem::path& projectDirectory)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    if ( editor )
    {
        editor->rebindProjectDirectory(projectDirectory);
    }
}

bool resourceAuthoringPaneIsModified(const QWidget* pane)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->isModified();
}

bool resourceAuthoringPaneSave(QWidget* pane, const bool saveAs)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->save(saveAs);
}

bool resourceAuthoringPaneOpenLooseFile(QWidget* pane)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->openLooseFileForCaller();
}

bool resourceAuthoringPaneOpenLooseFileDirect(QWidget* pane)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->openLooseFileForCaller(true);
}

bool resourceAuthoringPaneOpenProjectResource(QWidget* pane)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->openProjectResourceForCaller();
}

bool resourceAuthoringPaneOpenProjectResourceDirect(QWidget* pane)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->openProjectResourceForCaller(true);
}

bool resourceAuthoringPaneConfirmClose(QWidget* pane, bool* discardedAny)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->confirmClose(discardedAny);
}

void closeResourceAuthoringPaneAfterReplacement(QWidget* pane)
{
    if ( auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane) )
    {
        editor->closeAfterReplacement();
    }
}

bool resourceAuthoringPaneUndoRedo(QWidget* pane, const bool redo)
{
    auto* const editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->undoRedoDocument(redo, true);
}

bool resourceAuthoringPaneCanUndoRedo(const QWidget* pane, const bool redo)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->canUndoRedoDocument(redo);
}

bool resourceAuthoringPaneDocumentUndoRedo(QWidget* pane, const bool redo)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->undoRedoDocument(redo);
}

bool resourceAuthoringPaneOwnsFocus(const QWidget* pane)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->ownsFocus();
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
ResourceAuthoringPaneTestCounters resourceAuthoringPaneTestCounters(const QWidget* pane)
{
    const auto* editor = dynamic_cast<const detail::ResourceAuthoringDialog*>(pane);
    return editor ? editor->testCounters() : ResourceAuthoringPaneTestCounters{};
}

void resetResourceAuthoringPaneTestCounters(QWidget* pane)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    if ( editor )
    {
        editor->resetTestCounters();
    }
}

void setResourceAuthoringPaneAfterReadHook(
    QWidget* pane,
    ResourceAuthoringPaneAfterReadHook hook)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    if ( editor )
    {
        editor->setAfterReadHookForTest(std::move(hook));
    }
}

bool resourceAuthoringPaneReloadCurrentLooseSourceForTest(QWidget* pane, QString* error)
{
    auto* editor = dynamic_cast<detail::ResourceAuthoringDialog*>(pane);
    return editor && editor->reloadCurrentLooseSourceForTest(error);
}
#endif

} // namespace sed::ui::resources
