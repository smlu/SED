#include "ui/resources/resource_picker.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "app_logging.hpp"
#include "ui/resources/resource_picker_kind_utils.hpp"
#include "ui/resources/template_resource_utils.hpp"
#include "ui/audio/app_sound_player.hpp"
#include "ui/audio/wav_preview_metadata.hpp"
#include "ui/components/category_membership_utils.hpp"
#include "ui/components/category_tree_widgets.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"
#include "ui/components/preview_thumbnail_list.hpp"
#include "ui/components/shared_ui_styles.hpp"
#include "ui/components/template_model_preview_icon.hpp"
#include "ui/components/thing_preview_render_utils.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/components/viewport_icons.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"

#include "sed/assets/3do.hpp"
#include "sed/assets/cmp.hpp"
#include "sed/assets/jlh.hpp"
#include "sed/assets/mat.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMatrix4x4>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QScrollBar>
#include <QSizePolicy>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QVector3D>
#include <QWheelEvent>
#include <QWidget>
namespace sed::ui
{
namespace
{

enum class SourceKind : int
{
    all = 0,
    diskRoot = 1,
    gob = 2,
};

struct ResourceSource
{
    SourceKind kind{};
    int index{}; // root / gob index, -1 for all
    QString label{};
};

struct ResourceKindSpec
{
    QString displayName{};
    QString folderLabel{};
    std::vector<std::string> extensions{}; // lowercase with leading dot
    std::vector<std::string> folders{};    // lowercase, slash-separated, no leading/trailing slash
    bool showMatThumbs{};
    bool showModelPreview{};
    bool includeConfiguredDiskRoots{};
};

constexpr int kSourceKindRole = Qt::UserRole + 210;
constexpr int kSourceIndexRole = Qt::UserRole + 211;
constexpr int kActualNameRole = Qt::UserRole + 212;
constexpr int kTemplateCategoryNamesRole = Qt::UserRole + 213;
constexpr int kCategoryRootRole = Qt::UserRole + 215;
constexpr int kCategoryPathRole = Qt::UserRole + 216;
constexpr int kCategoryHasListRole = Qt::UserRole + 217;
constexpr int kMatThumbCacheResolution = 160;
constexpr int kInitialPopulationDelayMs = 16;
constexpr int kPreviewThumbStateRole = detail::kPreviewThumbStateRole;
constexpr auto kSoundPreviewVolumeSettingsKey = "resource_picker/sound_preview_volume_percent";
using PreviewThumbState = detail::PreviewThumbState;

QString formatSoundDuration(const std::chrono::microseconds duration)
{
    const auto totalMilliseconds = std::max<std::int64_t>(
        0,
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    if (totalMilliseconds >= 60'000)
    {
        const auto minutes = totalMilliseconds / 60'000;
        const double seconds = static_cast<double>(totalMilliseconds % 60'000) / 1'000.0;
        return QStringLiteral("%1:%2").arg(
            uiNumericLocale().toString(static_cast<qlonglong>(minutes)),
            formatUiNumber(seconds, 'f', 2).rightJustified(5, uiNumericLocale().zeroDigit().at(0)));
    }

    const double seconds = static_cast<double>(totalMilliseconds) / 1'000.0;
    return QStringLiteral("%1 s").arg(formatUiNumber(seconds, 'f', seconds < 10.0 ? 2 : 1));
}

enum class CategoryEditState : std::uint8_t
{
    none = 0,
    creating = 1,
    renaming = 2,
};

enum class MatThumbLayout : int
{
    none = 0,
    small = 1,
    middle = 2,
    large = 3,
};

enum class PickerSortMode : int
{
    nameAscending = 0,
    nameDescending = 1,
    sourceOrder = 2,
};

class PickerTileSelectionDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto* listView = qobject_cast<const QListView*>(option.widget);
        if (!listView)
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        if (listView->viewMode() == QListView::ListMode)
        {
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            opt.state &= ~QStyle::State_MouseOver;

            const bool selected = (opt.state & QStyle::State_Selected) != 0;
            const bool enabled = (opt.state & QStyle::State_Enabled) != 0;
            const bool active = (opt.state & QStyle::State_Active) != 0;
            const QRect textRect = opt.rect.adjusted(10, 0, -8, 0);
            const QFontMetrics metrics(opt.font);
            const QString elided = metrics.elidedText(
                opt.text,
                Qt::ElideRight,
                std::max(0, textRect.width()));
            // In wrapped ListMode, opt.rect is the entire fixed-width column cell.
            // Highlighting that whole rectangle makes a short filename look like a
            // selected empty row stretching into the next column. Keep the marker
            // tight to the text so there is exactly one unambiguous selected entry.
            const int selectionWidth = std::min(
                std::max(0, opt.rect.width() - 16),
                metrics.horizontalAdvance(elided) + 16);
            const QRect selectionRect(
                opt.rect.left() + 4,
                opt.rect.top() + 1,
                selectionWidth,
                std::max(0, opt.rect.height() - 2));

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setClipRect(opt.rect);
            painter->fillRect(opt.rect, opt.palette.brush(QPalette::Base));
            if (selected && !selectionRect.isEmpty())
            {
                QColor fill = neutralListSelectionFillColor(active);
                fill.setAlpha(active ? 168 : 132);
                painter->setPen(Qt::NoPen);
                painter->setBrush(fill);
                painter->drawRoundedRect(selectionRect, 3.0, 3.0);
            }

            painter->setFont(opt.font);
            painter->setPen(enabled
                ? (selected ? QColor(Qt::white) : opt.palette.color(QPalette::Text))
                : opt.palette.color(QPalette::Disabled, QPalette::Text));
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elided);
            painter->restore();
            return;
        }

        if (listView->viewMode() != QListView::IconMode)
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const QFontMetrics metrics(opt.font);
        const QSize iconSize = opt.decorationSize.isValid() ? opt.decorationSize : QSize(96, 96);
        const int maxTextWidth = std::max(48, opt.rect.width() - 18);
        const QRect textBounds = metrics.boundingRect(
            QRect(0, 0, maxTextWidth, 0),
            Qt::AlignHCenter | Qt::TextWordWrap,
            opt.text);

        const int cardInnerWidth = std::max(iconSize.width(), textBounds.width());
        const int cardWidth = std::min(opt.rect.width() - 10, cardInnerWidth + 12);
        const int cardHeight = std::min(opt.rect.height() - 6, iconSize.height() + textBounds.height() + 12);
        const int cardX = opt.rect.x() + ((opt.rect.width() - cardWidth) / 2);
        const int cardY = opt.rect.y() + 1;
        const QRect cardRect(cardX, cardY, cardWidth, cardHeight);
        const QRect contentRect = cardRect.adjusted(5, 2, -5, -4);
        const bool selected = (opt.state & QStyle::State_Selected) != 0;
        const bool enabled = (opt.state & QStyle::State_Enabled) != 0;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setClipRect(opt.rect);
        painter->fillRect(opt.rect, opt.palette.brush(QPalette::Base));

        if (selected)
        {
            painter->setPen(QPen(neutralListSelectionBorderColor(), 1.0));
            painter->setBrush(neutralListSelectionFillColor(true));
            painter->drawRoundedRect(cardRect, 8.0, 8.0);
        }

        const int iconX = contentRect.x() + ((contentRect.width() - iconSize.width()) / 2);
        const QRect iconRect(iconX, contentRect.y(), iconSize.width(), iconSize.height());
        const QRect finalTextRect(
            contentRect.x(),
            iconRect.bottom() + 4,
            contentRect.width(),
            std::max(0, contentRect.bottom() - (iconRect.bottom() + 4) + 1));

        opt.icon.paint(
            painter,
            iconRect,
            Qt::AlignCenter,
            enabled ? QIcon::Normal : QIcon::Disabled,
            (opt.state & QStyle::State_Open) != 0 ? QIcon::On : QIcon::Off);

        const QColor textColor = enabled
            ? (selected ? Qt::white : opt.palette.color(QPalette::Text))
            : opt.palette.color(QPalette::Disabled, QPalette::Text);
        painter->setPen(textColor);
        QTextOption textOption(Qt::AlignHCenter);
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter->drawText(finalTextRect, opt.text, textOption);

        painter->restore();
    }
};

ResourceKindSpec specForKind(const ResourcePickerKind kind)
{
    switch (kind)
    {
        case ResourcePickerKind::cog:
            return ResourceKindSpec{
                .displayName      = "COG script",
                .folderLabel      = "COG",
                .extensions       = { ".cog" },
                .folders          = { "cog" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::material:
            return ResourceKindSpec{
                .displayName      = "Material",
                .folderLabel      = "mat",
                .extensions       = { ".mat" },
                .folders          = { "mat", "3do/mat" },
                .showMatThumbs    = true,
                .showModelPreview = false,
            };
        case ResourcePickerKind::model3do:
            return ResourceKindSpec{
                .displayName      = "3DO model",
                .folderLabel      = "3do",
                .extensions       = { ".3do" },
                .folders          = { "3do", "hi3do" },
                .showMatThumbs    = false,
                .showModelPreview = true,
            };
        case ResourcePickerKind::key:
            return ResourceKindSpec{
                .displayName      = "Keyframe",
                .folderLabel      = "key",
                .extensions       = { ".key" },
                .folders          = { "3do/key", "key", "misc/key" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::wav:
            return ResourceKindSpec{
                .displayName      = "Wave",
                .folderLabel      = "sound",
                .extensions       = { ".wav" },
                .folders          = { "sound", "voice", "voiceuu", "wav", "" },
                .showMatThumbs    = false,
                .showModelPreview = false,
                .includeConfiguredDiskRoots = true,
            };
        case ResourcePickerKind::ai:
            return ResourceKindSpec{
                .displayName      = "AI class",
                .folderLabel      = "ai",
                .extensions       = { ".ai", ".ai0", ".ai2" },
                .folders          = { "misc/ai", "ai" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::template_:
            return ResourceKindSpec{
                .displayName      = "Template",
                .folderLabel      = "tpl",
                .extensions       = { ".tpl" },
                .folders          = { "misc", "template", "misc/template", "tpl" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::message:
            return ResourceKindSpec{
                .displayName      = "Message",
                .folderLabel      = "msg",
                .extensions       = { ".msg" },
                .folders          = { "misc/msg", "msg" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::cmp:
            return ResourceKindSpec{
                .displayName      = "Colormap",
                .folderLabel      = "cmp",
                .extensions       = { ".cmp" },
                .folders          = { "misc/cmp", "cmp" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::snd:
            return ResourceKindSpec{
                .displayName      = "Sound class",
                .folderLabel      = "snd",
                .extensions       = { ".snd" },
                .folders          = { "misc/snd", "snd" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::pup:
            return ResourceKindSpec{
                .displayName      = "Puppet",
                .folderLabel      = "pup",
                .extensions       = { ".pup" },
                .folders          = { "misc/pup", "pup" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::spr:
            return ResourceKindSpec{
                .displayName      = "Sprite",
                .folderLabel      = "spr",
                .extensions       = { ".spr" },
                .folders          = { "misc/spr", "spr" },
                .showMatThumbs    = true,
                .showModelPreview = true,
            };
        case ResourcePickerKind::par:
            return ResourceKindSpec{
                .displayName      = "Particle",
                .folderLabel      = "par",
                .extensions       = { ".par" },
                .folders          = { "misc/par", "par" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
        case ResourcePickerKind::uni:
            return ResourceKindSpec{
                .displayName      = "String Table",
                .folderLabel      = "uni",
                .extensions       = { ".uni" },
                .folders          = { "misc", "uni" },
                .showMatThumbs    = false,
                .showModelPreview = false,
            };
    }

    return ResourceKindSpec{
        .displayName      = "Resource",
        .folderLabel      = "resource",
        .extensions       = {},
        .folders          = {},
        .showMatThumbs    = false,
        .showModelPreview = false,
    };
}

bool isBaseTemplateName(const QString& name)
{
    const QString trimmed = name.trimmed();
    return !trimmed.isEmpty()
        && trimmed.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0
        && trimmed.startsWith(QLatin1Char('_'));
}

std::optional<QString> categoryFileNameForPickerKind(
    const ResourcePickerKind kind,
    const ProjectType projectKind)
{
    const auto fileName = resourcePickerCategoryFileName(kind, projectKind);
    if (!fileName)
    {
        return std::nullopt;
    }

    return QString::fromLatin1(fileName->data(), static_cast<qsizetype>(fileName->size()));
}

bool hasCompleteCategoryFilePair(const std::filesystem::path& jlhPath)
{
    if (jlhPath.empty())
    {
        return false;
    }

    std::filesystem::path jllPath = jlhPath;
    jllPath.replace_extension(".jll");

    std::error_code jlhEc;
    std::error_code jllEc;
    return std::filesystem::is_regular_file(jlhPath, jlhEc) && !jlhEc
        && std::filesystem::is_regular_file(jllPath, jllEc) && !jllEc;
}

std::optional<assets::JlhTree> tryLoadCategoryFilePair(const std::filesystem::path& jlhPath)
{
    if (!hasCompleteCategoryFilePair(jlhPath))
    {
        return std::nullopt;
    }

    try
    {
        return assets::loadFromJLH(jlhPath);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

std::optional<assets::JlhTree> loadTemplateCategoryTree(
    const sed::assets::ResourceLocator& resources,
    const ResourcePickerKind pickerKind,
    const ProjectType projectKind,
    const std::optional<std::filesystem::path>& projectDirectory)
{
    const std::optional<QString> fileName = categoryFileNameForPickerKind(pickerKind, projectKind);
    if (!fileName)
    {
        return std::nullopt;
    }

    // Legacy SED lets a saved level project override the editor-wide category
    // files. Require the JLH and JLL to live together so an incomplete project
    // override cannot accidentally pair with a shared list file.
    if (projectDirectory && !projectDirectory->empty())
    {
        if (const auto tree = tryLoadCategoryFilePair(*projectDirectory / fileName->toStdString()))
        {
            return tree;
        }
    }

    // Keep the existing editable editor data as the shared fallback.
    if (const auto dataRoot = findDataRoot())
    {
        const std::filesystem::path dataPath = *dataRoot / fileName->toStdString();
        if (const auto tree = tryLoadCategoryFilePair(dataPath))
        {
            return tree;
        }
    }

    // A configured disk root can also carry an explicit pair. Load it by path
    // to keep both halves of the category model at the same origin.
    if (const auto diskPath = resources.findOnDisk(fileName->toStdString()))
    {
        if (const auto tree = tryLoadCategoryFilePair(*diskPath))
        {
            return tree;
        }
    }

    // Finally retain support for category pairs mounted in an archive.
    try
    {
        return assets::loadFromJLH(fileName->toStdString(), resources);
    }
    catch (const std::exception&)
    {
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> editableCategoryTreePath(
    const ResourcePickerKind pickerKind,
    const ProjectType projectKind,
    const std::optional<std::filesystem::path>& projectDirectory)
{
    const std::optional<QString> fileName = categoryFileNameForPickerKind(pickerKind, projectKind);
    if (!fileName)
    {
        return std::nullopt;
    }

    if (projectDirectory && !projectDirectory->empty())
    {
        return *projectDirectory / fileName->toStdString();
    }

    const auto dataRoot = findDataRoot();
    if (!dataRoot)
    {
        return std::nullopt;
    }

    return *dataRoot / fileName->toStdString();
}

QString encodeCategoryPath(const std::vector<std::size_t>& path)
{
    QStringList parts{};
    for (const std::size_t index : path)
    {
        parts.push_back(QString::number(static_cast<qulonglong>(index)));
    }
    return parts.join(QLatin1Char('/'));
}

std::vector<std::size_t> decodeCategoryPath(const QString& encoded)
{
    std::vector<std::size_t> path{};
    const QStringList parts = encoded.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    path.reserve(static_cast<std::size_t>(parts.size()));
    for (const QString& part : parts)
    {
        bool ok = false;
        const qulonglong value = part.toULongLong(&ok);
        if (!ok)
        {
            return {};
        }
        path.push_back(static_cast<std::size_t>(value));
    }
    return path;
}

assets::JlhNode* categoryNodeAt(assets::JlhTree& tree, const std::vector<std::size_t>& path) noexcept
{
    std::vector<assets::JlhNode>* siblings = &tree.roots;
    assets::JlhNode* current = nullptr;
    for (const std::size_t index : path)
    {
        if (index >= siblings->size())
        {
            return nullptr;
        }
        current = &(*siblings)[index];
        siblings = &current->children;
    }
    return current;
}

const assets::JlhNode* categoryNodeAt(const assets::JlhTree& tree, const std::vector<std::size_t>& path) noexcept
{
    const std::vector<assets::JlhNode>* siblings = &tree.roots;
    const assets::JlhNode* current = nullptr;
    for (const std::size_t index : path)
    {
        if (index >= siblings->size())
        {
            return nullptr;
        }
        current = &(*siblings)[index];
        siblings = &current->children;
    }
    return current;
}

std::vector<assets::JlhNode>* categorySiblingsAt(
    assets::JlhTree& tree,
    const std::vector<std::size_t>& parentPath) noexcept
{
    if (parentPath.empty())
    {
        return &tree.roots;
    }

    if (assets::JlhNode* parent = categoryNodeAt(tree, parentPath))
    {
        return &parent->children;
    }

    return nullptr;
}

void collectCategoryListNames(
    const std::vector<assets::JlhNode>& nodes,
    std::unordered_set<std::string>& out)
{
    for (const assets::JlhNode& node : nodes)
    {
        if (node.list)
        {
            out.insert(sed::text::upperAscii(node.list->name));
        }
        if (!node.children.empty())
        {
            collectCategoryListNames(node.children, out);
        }
    }
}

std::string makeCategoryListKey(const assets::JlhTree& tree, const QString& label)
{
    std::string base{};
    base.reserve(static_cast<std::size_t>(label.size()));
    for (const QChar ch : label)
    {
        if (ch.isLetterOrNumber())
        {
            base.push_back(static_cast<char>(std::tolower(ch.toLatin1())));
        }
        else if (ch.isSpace() || ch == QLatin1Char('-') || ch == QLatin1Char('_'))
        {
            if (base.empty() || base.back() != '_')
            {
                base.push_back('_');
            }
        }
    }

    while (!base.empty() && base.back() == '_')
    {
        base.pop_back();
    }
    if (base.empty())
    {
        base = "category";
    }

    std::unordered_set<std::string> used{};
    collectCategoryListNames(tree.roots, used);

    std::string candidate = base;
    int suffix = 2;
    while (used.contains(sed::text::upperAscii(candidate)))
    {
        candidate = base + "_" + std::to_string(suffix++);
    }
    return candidate;
}

bool categoryListContainsEntry(const assets::JllList& list, const QString& value)
{
    return detail::categoryMembershipContains(list, value);
}

const assets::JllList* assignableCategoryList(const assets::JlhNode& node) noexcept
{
    return detail::assignableCategoryMembershipList(node);
}

bool isInternalGeneralCategoryNode(const assets::JlhNode& node) noexcept
{
    return detail::isInternalGeneralCategoryMembershipNode(node);
}

std::size_t visibleCategoryChildCount(const assets::JlhNode& node) noexcept
{
    std::size_t count = 0;
    for (const assets::JlhNode& child : node.children)
    {
        if (!isInternalGeneralCategoryNode(child))
        {
            ++count;
        }
    }
    return count;
}

struct AssetPreviewSpinSettings
{
    bool enabled{ true };
    float speedDegPerSec{ 72.0f };
};

enum class TemplateListModelPreviewMode : int
{
    shaded = 0,
    wireframe = 1,
    none = 2,
};

constexpr float kAssetPreviewDefaultYawDeg = 160.0f;
constexpr float kAssetPreviewDefaultPitchDeg = 14.0f;
constexpr float kAssetPreviewDefaultRollDeg = 0.0f;
constexpr float kAssetPreviewFitPadding = 1.24f;

AssetPreviewSpinSettings loadAssetPreviewSpinSettings()
{
    QSettings settings = settingsStore();
    AssetPreviewSpinSettings out{};
    out.enabled = settings.value("preview/asset_auto_rotate_enabled", true).toBool();
    out.speedDegPerSec = std::clamp(
        static_cast<float>(settings.value("preview/asset_auto_rotate_speed_deg_per_sec", 72.0).toDouble()),
        0.0f,
        720.0f);
    return out;
}

TemplateListModelPreviewMode loadTemplateListModelPreviewMode()
{
    QSettings settings = settingsStore();
    constexpr auto kSettingKey = "preview/template_list_3do_thumb_mode";
    const QVariant storedValue = settings.value(
        kSettingKey,
        static_cast<int>(TemplateListModelPreviewMode::shaded));
    const detail::TemplateThumbnailModeSetting normalized =
        detail::normalizeTemplateThumbnailModeSetting(storedValue);
    if (normalized.migrate)
    {
        settings.setValue(kSettingKey, normalized.mode);
    }
    return static_cast<TemplateListModelPreviewMode>(normalized.mode);
}

std::optional<assets::Templates> loadTemplatePickerDatabase(
    const sed::assets::ResourceLocator& resources,
    const ProjectType kind)
{
    const std::optional<QString> fileName = templateFileNameForProject(kind);
    if (!fileName)
    {
        return std::nullopt;
    }

    try
    {
        return assets::loadTemplates(resources, fileName->toStdString());
    }
    catch (const std::exception&)
    {
    }
    return std::nullopt;
}

void mergeTemplateDatabaseFallback(
    assets::Templates& database,
    const assets::Templates* fallback)
{
    if (!fallback)
    {
        return;
    }

    for (std::size_t i = 0; i < fallback->size(); ++i)
    {
        if (const ThingTemplate* tpl = fallback->get(i))
        {
            try
            {
                (void)database.addOrUpdate(*tpl, true);
            }
            catch (const std::exception&)
            {
            }
        }
    }
}

void mergeTemplateOverrides(
    assets::Templates& database,
    const std::vector<ThingTemplate>* overrides)
{
    if (!overrides)
    {
        return;
    }

    for (const ThingTemplate& tpl : *overrides)
    {
        try
        {
            (void)database.addOrUpdate(tpl, true);
        }
        catch (const std::exception&)
        {
        }
    }
}

QString normalizeEntrySlashes(const std::string_view name)
{
    QString s = QString::fromStdString(std::string(name));
    s.replace('\\', '/');
    while (s.startsWith('/'))
    {
        s.remove(0, 1);
    }
    while (s.endsWith('/'))
    {
        s.chop(1);
    }
    return s;
}

QString fileNameOnly(const QString& value)
{
    return QFileInfo(value.trimmed()).fileName().trimmed();
}

bool hasAnyExtension(const QString& name, const std::vector<std::string>& extensions)
{
    if (name.isEmpty())
    {
        return false;
    }
    for (const std::string& ext : extensions)
    {
        if (name.endsWith(QString::fromStdString(ext), Qt::CaseInsensitive))
        {
            return true;
        }
    }
    return false;
}

std::optional<QString> tryExtractBasenameFromEntry(const std::string_view entryName, const ResourceKindSpec& spec)
{
    if ( entryName.empty() )
    {
        return std::nullopt;
    }

    const QString original = normalizeEntrySlashes(entryName);
    if ( original.isEmpty() )
    {
        return std::nullopt;
    }

    const QString lower = original.toLower();
    for ( const std::string& folder : spec.folders )
    {
        if ( folder.empty() )
        {
            if ( !original.contains('/') && hasAnyExtension(original, spec.extensions) )
            {
                return original;
            }
            continue;
        }
        const QString prefix = QString::fromStdString(folder) + "/";
        if ( !lower.startsWith(prefix) )
        {
            continue;
        }

        const QString tail = original.mid(prefix.size());
        if ( tail.isEmpty() || tail.contains('/') )
        {
            continue;
        }
        if ( !hasAnyExtension(tail, spec.extensions) )
        {
            continue;
        }
        return tail;
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> collectDiskRoots(const sed::assets::ResourceLocator& resources,
                                                    const std::optional<std::filesystem::path>& preferredDiskRoot,
                                                    const bool includeConfiguredRoots = false)
{
    std::vector<std::filesystem::path> out{};

    auto pathsEquivalent = [](const std::filesystem::path& a, const std::filesystem::path& b) -> bool
    {
        if ( a.empty() || b.empty() )
        {
            return false;
        }

        std::error_code ecA;
        std::error_code ecB;
        const bool aExists = std::filesystem::exists(a, ecA) && !ecA;
        const bool bExists = std::filesystem::exists(b, ecB) && !ecB;
        if ( aExists && bExists )
        {
            std::error_code eqEc;
            if ( std::filesystem::equivalent(a, b, eqEc) && !eqEc )
            {
                return true;
            }
        }

        auto normalize = [](const std::filesystem::path& p) -> QString
        {
            std::error_code ec;
            const std::filesystem::path abs = std::filesystem::absolute(p, ec).lexically_normal();
            QString key = QString::fromStdWString((!ec ? abs : p).wstring()).toLower();
            key.replace('\\', '/');
            while ( key.endsWith('/') )
            {
                key.chop(1);
            }
            return key;
        };

        return normalize(a) == normalize(b);
    };

    auto addUnique = [&](const std::filesystem::path& p)
    {
        if ( p.empty() )
        {
            return;
        }
        for ( const std::filesystem::path& existing : out )
        {
            if ( pathsEquivalent(existing, p) )
            {
                return;
            }
        }
        out.push_back(p);
    };

    if ( preferredDiskRoot && !preferredDiskRoot->empty() )
    {
        addUnique(*preferredDiskRoot);
    }
    if ( !preferredDiskRoot || preferredDiskRoot->empty() || includeConfiguredRoots )
    {
        for ( const std::filesystem::path& root : resources.roots() )
        {
            addUnique(root);
        }
    }

    return out;
}

bool rootHasAnyResource(const std::filesystem::path& root, const ResourceKindSpec& spec)
{
    std::error_code ec;
    for (const std::string& folder : spec.folders)
    {
        const std::filesystem::path dir = root / std::filesystem::path(folder);
        if (!std::filesystem::exists(dir, ec) || ec)
        {
            continue;
        }
        if (!std::filesystem::is_directory(dir, ec) || ec)
        {
            continue;
        }

        for (const auto& de : std::filesystem::directory_iterator(dir, ec))
        {
            if (ec)
            {
                break;
            }
            if (!de.is_regular_file(ec) || ec)
            {
                continue;
            }

            const QString fileName = QString::fromStdWString(de.path().filename().wstring());
            if (hasAnyExtension(fileName, spec.extensions))
            {
                return true;
            }
        }
    }

    return false;
}

std::string resourceSourceCacheKey(const SourceKind kind, const int index)
{
    return std::to_string(static_cast<int>(kind)) + ":" + std::to_string(index);
}

std::string resourcePickerPathToken(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return "EMPTY";
    }

    std::error_code ec;
    const std::filesystem::path normalized =
        std::filesystem::absolute(path, ec).lexically_normal();
    QString key = QString::fromStdWString((ec ? path : normalized).wstring()).toLower();
    key.replace('\\', '/');
    while (key.endsWith('/'))
    {
        key.chop(1);
    }

    std::error_code existsEc;
    const bool exists = std::filesystem::exists(path, existsEc) && !existsEc;
    if (!exists)
    {
        return key.toUtf8().toStdString() + "|missing";
    }

    std::error_code timeEc;
    const auto stamp = std::filesystem::last_write_time(path, timeEc);
    std::uintmax_t size = 0;
    std::error_code regularEc;
    if (std::filesystem::is_regular_file(path, regularEc) && !regularEc)
    {
        std::error_code sizeEc;
        size = std::filesystem::file_size(path, sizeEc);
        if (sizeEc)
        {
            size = 0;
        }
    }

    return key.toUtf8().toStdString()
        + "|"
        + std::to_string(timeEc ? 0ll : stamp.time_since_epoch().count())
        + "|"
        + std::to_string(size);
}

std::string resourceKindCacheToken(const ResourceKindSpec& spec)
{
    std::string out = spec.displayName.toStdString();
    out += "|ext:";
    for (const std::string& ext : spec.extensions)
    {
        out += ext;
        out += ';';
    }
    out += "|folders:";
    for (const std::string& folder : spec.folders)
    {
        out += folder;
        out += ';';
    }
    return out;
}

std::string resourceCollectionCacheBaseKey(
    const sed::assets::ResourceLocator& resources,
    const ResourceKindSpec& spec,
    const std::optional<std::filesystem::path>& preferredDiskRoot)
{
    std::string key = resourceKindCacheToken(spec);
    key += "|roots:";
    for ( const std::filesystem::path& root :
          collectDiskRoots(resources, preferredDiskRoot, spec.includeConfiguredDiskRoots) )
    {
        key += resourcePickerPathToken(root);
        key += '{';
        for (const std::string& folder : spec.folders)
        {
            key += folder;
            key += '=';
            key += resourcePickerPathToken(root / std::filesystem::path(folder));
            key += ';';
        }
        key += '}';
    }
    key += "|mount=" + std::to_string(resources.previewCacheIdentity());
    key += "|gobs:";
    for (const assets::GobArchive& gob : resources.gobs())
    {
        key += resourcePickerPathToken(gob.path());
        key += ';';
    }
    return key;
}

std::unordered_map<std::string, std::vector<ResourceSource>>& sharedResourceSourceListCache()
{
    static std::unordered_map<std::string, std::vector<ResourceSource>> cache{};
    return cache;
}

std::unordered_map<std::string, std::vector<QString>>& sharedResourceListCache()
{
    static std::unordered_map<std::string, std::vector<QString>> cache{};
    return cache;
}

std::unordered_map<std::string, std::unordered_set<std::string>>& sharedDiskResourceNameCache()
{
    static std::unordered_map<std::string, std::unordered_set<std::string>> cache{};
    return cache;
}

std::unordered_set<std::string> collectDiskResourceNameKeys(
    const sed::assets::ResourceLocator& resources,
    const ResourceKindSpec& spec)
{
    const std::string cacheKey = resourceCollectionCacheBaseKey(resources, spec, std::nullopt)
        + "|disk-resource-names";
    if (const auto it = sharedDiskResourceNameCache().find(cacheKey); it != sharedDiskResourceNameCache().end())
    {
        return it->second;
    }

    std::unordered_set<std::string> names{};
    auto scanDirectory = [&](const std::filesystem::path& directory)
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(directory, ec) || ec)
        {
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
        {
            if (ec)
            {
                break;
            }
            if (!entry.is_regular_file(ec) || ec)
            {
                continue;
            }

            const QString fileName = QString::fromStdWString(entry.path().filename().wstring());
            if (hasAnyExtension(fileName, spec.extensions))
            {
                names.insert(sed::text::upperAscii(fileNameOnly(fileName).toStdString()));
            }
        }
    };

    // ResourceLocator::findOnDisk() searches the root itself followed by the
    // kind-specific folders. Index those locations once instead of issuing
    // several filesystem probes for every visible picker row.
    for (const std::filesystem::path& root : resources.roots())
    {
        scanDirectory(root);
        for (const std::string& folder : spec.folders)
        {
            scanDirectory(root / std::filesystem::path(folder));
        }
    }

    auto& cache = sharedDiskResourceNameCache();
    if (cache.size() >= 64u && cache.find(cacheKey) == cache.end())
    {
        cache.erase(cache.begin());
    }
    cache[cacheKey] = names;
    return names;
}

std::vector<ResourceSource> collectSources(
    const sed::assets::ResourceLocator& resources,
    const ResourceKindSpec& spec,
    const std::optional<std::filesystem::path>& preferredDiskRoot)
{
    const std::string cacheKey = resourceCollectionCacheBaseKey(resources, spec, preferredDiskRoot) + "|sources";
    if (const auto it = sharedResourceSourceListCache().find(cacheKey); it != sharedResourceSourceListCache().end())
    {
        return it->second;
    }

    std::vector<ResourceSource> out{};
    out.push_back(ResourceSource{ SourceKind::all, -1, QStringLiteral("All") });

    for (std::size_t i = 0; i < resources.gobs().size(); ++i)
    {
        const assets::GobArchive& gob = resources.gobs()[i];
        bool hasAny = false;
        for (const assets::GobEntry& entry : gob.entries())
        {
            if (tryExtractBasenameFromEntry(entry.name, spec))
            {
                hasAny = true;
                break;
            }
        }
        if (!hasAny)
        {
            continue;
        }

        const QString gobLabel = QString::fromStdWString(gob.path().filename().wstring());
        out.push_back(ResourceSource{
            SourceKind::gob,
            static_cast<int>(i),
            QString("%1>%2").arg(gobLabel, spec.folderLabel) });
    }

    const std::vector<std::filesystem::path> roots =
        collectDiskRoots(resources, preferredDiskRoot, spec.includeConfiguredDiskRoots);
    for (std::size_t i = 0; i < roots.size(); ++i)
    {
        const std::filesystem::path& root = roots[i];
        if (!rootHasAnyResource(root, spec))
        {
            continue;
        }

        QString rootLabel = QString::fromStdWString(root.filename().wstring());
        if (rootLabel.isEmpty())
        {
            rootLabel = QString::fromStdWString(root.wstring());
        }

        out.push_back(ResourceSource{
            SourceKind::diskRoot,
            static_cast<int>(i),
            QString("%1>%2").arg(rootLabel, spec.folderLabel) });
    }

    auto& cache = sharedResourceSourceListCache();
    if (cache.size() >= 64u && cache.find(cacheKey) == cache.end())
    {
        cache.erase(cache.begin());
    }
    cache[cacheKey] = out;
    return out;
}

std::vector<std::filesystem::path> candidateDiskPaths(
    const std::filesystem::path& root,
    const ResourceKindSpec& spec,
    const std::string_view name)
{
    const QString qName = fileNameOnly(QString::fromStdString(std::string(name)));
    if (qName.isEmpty())
    {
        return {};
    }

    const std::string base = qName.toStdString();
    std::vector<std::filesystem::path> out{};
    out.reserve(spec.folders.size());
    for (const std::string& folder : spec.folders)
    {
        out.push_back(root / std::filesystem::path(folder) / std::filesystem::path(base));
    }
    return out;
}

std::vector<std::string> candidateGobEntries(const ResourceKindSpec& spec, const std::string_view name)
{
    const QString qName = fileNameOnly(QString::fromStdString(std::string(name)));
    if (qName.isEmpty())
    {
        return {};
    }

    const std::string base = qName.toStdString();
    std::vector<std::string> out{};
    out.reserve(spec.folders.size());
    for (const std::string& folder : spec.folders)
    {
        out.push_back(folder + "/" + base);
    }
    return out;
}

std::optional<QString> storageMetadataForDiskPath(const std::filesystem::path& path)
{
    std::error_code ec;
    if ( !std::filesystem::is_regular_file(path, ec) || ec )
    {
        return std::nullopt;
    }

    ec.clear();
    const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
    const std::filesystem::path& displayPath = ec ? path : absolute.lexically_normal();
    return QStringLiteral("Storage: %1")
        .arg(QString::fromStdWString(displayPath.wstring()));
}

std::optional<QString> storageMetadataForGobEntry(
    const assets::GobArchive& archive,
    const std::string_view candidate)
{
    const std::optional<assets::GobEntry> entry = archive.find(candidate);
    if ( !entry )
    {
        return std::nullopt;
    }

    const QString archiveName = QString::fromStdWString(archive.path().filename().wstring());
    QString entryName = QString::fromLatin1(entry->name.data(), static_cast<qsizetype>(entry->name.size()));
    entryName.replace('\\', '/');
    return QStringLiteral("Storage: %1: %2")
        .arg(archiveName, entryName);
}

QString storageMetadataForResource(
    const assets::ResourceLocator& resources,
    const ResourceKindSpec& spec,
    const SourceKind sourceKind,
    const int sourceIndex,
    const std::optional<std::filesystem::path>& preferredDiskRoot,
    const std::string_view name)
{
    if ( name.empty() )
    {
        return {};
    }

    if ( sourceKind == SourceKind::all
        && resources.primaryResourceOverrideRevision(name).has_value() )
    {
        return QStringLiteral("Storage: Project data");
    }

    if ( sourceKind == SourceKind::diskRoot )
    {
        const std::vector<std::filesystem::path> roots =
            collectDiskRoots(resources, preferredDiskRoot, spec.includeConfiguredDiskRoots);
        if ( sourceIndex < 0 || static_cast<std::size_t>(sourceIndex) >= roots.size() )
        {
            return QStringLiteral("Storage: Unavailable");
        }

        const std::filesystem::path& root = roots[static_cast<std::size_t>(sourceIndex)];
        for ( const std::filesystem::path& candidate : candidateDiskPaths(root, spec, name) )
        {
            if ( const auto metadata = storageMetadataForDiskPath(candidate) )
            {
                return *metadata;
            }
        }
        return QStringLiteral("Storage: Unavailable");
    }

    if ( sourceKind == SourceKind::gob )
    {
        if ( sourceIndex < 0 || static_cast<std::size_t>(sourceIndex) >= resources.gobs().size() )
        {
            return QStringLiteral("Storage: Unavailable");
        }

        const assets::GobArchive& archive = resources.gobs()[static_cast<std::size_t>(sourceIndex)];
        for ( const std::string& candidate : candidateGobEntries(spec, name) )
        {
            if ( const auto metadata = storageMetadataForGobEntry(archive, candidate) )
            {
                return *metadata;
            }
        }
        return QStringLiteral("Storage: Unavailable");
    }

    // Match ResourceLocator::tryReadBinary without reading payload bytes:
    // each canonical candidate checks configured roots and then mounted
    // archives, with the most recently added source winning within each set.
    for ( const std::string& candidate : resources.candidates(name) )
    {
        const std::filesystem::path candidatePath(candidate);
        if ( candidatePath.is_absolute() )
        {
            if ( const auto metadata = storageMetadataForDiskPath(candidatePath) )
            {
                return *metadata;
            }
            continue;
        }

        for ( auto root = resources.roots().rbegin(); root != resources.roots().rend(); ++root )
        {
            if ( const auto metadata = storageMetadataForDiskPath(*root / candidatePath) )
            {
                return *metadata;
            }
        }
        for ( auto archive = resources.gobs().rbegin(); archive != resources.gobs().rend(); ++archive )
        {
            if ( const auto metadata = storageMetadataForGobEntry(*archive, candidate) )
            {
                return *metadata;
            }
        }
    }

    // Resource pickers also expose kind-specific locations such as misc/snd
    // that are not aliases known to ResourceLocator. Preserve that existing
    // fallback order for rows that the general project lookup did not find.
    for ( const std::filesystem::path& root :
         collectDiskRoots(resources, preferredDiskRoot, spec.includeConfiguredDiskRoots) )
    {
        for ( const std::filesystem::path& candidate : candidateDiskPaths(root, spec, name) )
        {
            if ( const auto metadata = storageMetadataForDiskPath(candidate) )
            {
                return *metadata;
            }
        }
    }
    for ( const assets::GobArchive& archive : resources.gobs() )
    {
        for ( const std::string& candidate : candidateGobEntries(spec, name) )
        {
            if ( const auto metadata = storageMetadataForGobEntry(archive, candidate) )
            {
                return *metadata;
            }
        }
    }

    return QStringLiteral("Storage: Unavailable");
}

std::optional<std::vector<std::byte>> readBinaryFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return std::nullopt;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<std::byte> out{};
    if (size > 0)
    {
        out.resize(static_cast<std::size_t>(size));
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    }
    else
    {
        std::vector<char> rawBytes{};
        rawBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>{});
        out.resize(rawBytes.size());
        if (!rawBytes.empty())
        {
            std::copy_n(
                reinterpret_cast<const std::byte*>(rawBytes.data()),
                rawBytes.size(),
                out.data());
        }
    }

    if (!in && !out.empty())
    {
        return std::nullopt;
    }
    return out;
}

std::optional<std::vector<std::byte>> tryReadBinaryFromSource(
    const sed::assets::ResourceLocator& resources,
    const ResourceKindSpec& spec,
    const SourceKind sourceKind,
    const int sourceIndex,
    const std::optional<std::filesystem::path>& preferredDiskRoot,
    const std::string_view name,
    QString* sourceDescription = nullptr)
{
    if (sourceDescription) *sourceDescription = QStringLiteral("Storage: Unavailable");
    if (sourceKind == SourceKind::all)
    {
        try
        {
            if (const auto any = resources.tryReadBinary(name))
            {
                if (sourceDescription)
                {
                    *sourceDescription = storageMetadataForResource(
                        resources, spec, sourceKind, sourceIndex, preferredDiskRoot, name);
                }
                return any;
            }
        }
        catch (const std::exception&)
        {
            // The winning source exists but is unreadable. Do not replace
            // its preview with a different source's same-named bytes.
            return std::nullopt;
        }

        for ( const std::filesystem::path& root :
              collectDiskRoots(resources, preferredDiskRoot, spec.includeConfiguredDiskRoots) )
        {
            for (const std::filesystem::path& candidate : candidateDiskPaths(root, spec, name))
            {
                if (const auto bytes = readBinaryFile(candidate))
                {
                    if (sourceDescription)
                    {
                        *sourceDescription = storageMetadataForDiskPath(candidate)
                            .value_or(QStringLiteral("Storage: Unavailable"));
                    }
                    return bytes;
                }
            }
        }

        for (const assets::GobArchive& gob : resources.gobs())
        {
            for (const std::string& entry : candidateGobEntries(spec, name))
            {
                try
                {
                    if (gob.contains(entry))
                    {
                        auto bytes = gob.read(entry);
                        if (sourceDescription)
                        {
                            *sourceDescription = storageMetadataForGobEntry(gob, entry)
                                .value_or(QStringLiteral("Storage: Unavailable"));
                        }
                        return bytes;
                    }
                }
                catch (...)
                {
                }
            }
        }

        return std::nullopt;
    }

    if (sourceKind == SourceKind::diskRoot)
    {
        const std::vector<std::filesystem::path> roots =
            collectDiskRoots(resources, preferredDiskRoot, spec.includeConfiguredDiskRoots);
        if (sourceIndex < 0 || static_cast<std::size_t>(sourceIndex) >= roots.size())
        {
            return std::nullopt;
        }

        const std::filesystem::path& root = roots[static_cast<std::size_t>(sourceIndex)];
        for (const std::filesystem::path& candidate : candidateDiskPaths(root, spec, name))
        {
            if (const auto bytes = readBinaryFile(candidate))
            {
                if (sourceDescription)
                {
                    *sourceDescription = storageMetadataForDiskPath(candidate)
                        .value_or(QStringLiteral("Storage: Unavailable"));
                }
                return bytes;
            }
        }
        return std::nullopt;
    }

    if (sourceKind == SourceKind::gob)
    {
        if (sourceIndex < 0 || static_cast<std::size_t>(sourceIndex) >= resources.gobs().size())
        {
            return std::nullopt;
        }

        const assets::GobArchive& gob = resources.gobs()[static_cast<std::size_t>(sourceIndex)];
        for (const std::string& entry : candidateGobEntries(spec, name))
        {
            try
            {
                if (gob.contains(entry))
                {
                    auto bytes = gob.read(entry);
                    if (sourceDescription)
                    {
                        *sourceDescription = storageMetadataForGobEntry(gob, entry)
                            .value_or(QStringLiteral("Storage: Unavailable"));
                    }
                    return bytes;
                }
            }
            catch (...)
            {
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> tryReadTextFromSource(
    const sed::assets::ResourceLocator& resources,
    const ResourceKindSpec& spec,
    const SourceKind sourceKind,
    const int sourceIndex,
    const std::optional<std::filesystem::path>& preferredDiskRoot,
    const std::string_view name)
{
    const auto bytes = tryReadBinaryFromSource(resources, spec, sourceKind, sourceIndex, preferredDiskRoot, name);
    if (!bytes)
    {
        return std::nullopt;
    }
    if (bytes->empty())
    {
        return std::string{};
    }

    return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

std::vector<QString> collectResourcesForSource(
    const sed::assets::ResourceLocator& resources,
    const ResourceKindSpec& spec,
    const SourceKind sourceKind,
    const int sourceIndex,
    const std::optional<std::filesystem::path>& preferredDiskRoot)
{
    const std::string cacheKey =
        resourceCollectionCacheBaseKey(resources, spec, preferredDiskRoot)
        + "|resources|"
        + resourceSourceCacheKey(sourceKind, sourceIndex);
    if (const auto it = sharedResourceListCache().find(cacheKey); it != sharedResourceListCache().end())
    {
        return it->second;
    }

    std::unordered_set<std::string> seen{};
    std::vector<QString> out{};

    auto addName = [&](const QString& name)
    {
        const QString base = fileNameOnly(name);
        if (base.isEmpty() || !hasAnyExtension(base, spec.extensions))
        {
            return;
        }

        const std::string key = sed::text::upperAscii(base.toStdString());
        if (!seen.insert(key).second)
        {
            return;
        }
        out.push_back(base);
    };

    auto scanGob = [&](const assets::GobArchive& gob)
    {
        for (const assets::GobEntry& entry : gob.entries())
        {
            const auto name = tryExtractBasenameFromEntry(entry.name, spec);
            if (!name)
            {
                continue;
            }
            addName(*name);
        }
    };

    auto scanRoot = [&](const std::filesystem::path& root)
    {
        std::error_code ec;
        for (const std::string& folder : spec.folders)
        {
            const std::filesystem::path dir = root / std::filesystem::path(folder);
            if (!std::filesystem::exists(dir, ec) || ec)
            {
                continue;
            }
            if (!std::filesystem::is_directory(dir, ec) || ec)
            {
                continue;
            }

            for (const auto& de : std::filesystem::directory_iterator(dir, ec))
            {
                if (ec)
                {
                    break;
                }
                if (!de.is_regular_file(ec) || ec)
                {
                    continue;
                }
                addName(QString::fromStdWString(de.path().filename().wstring()));
            }
        }
    };

    if (sourceKind == SourceKind::all || sourceKind == SourceKind::gob)
    {
        if (sourceKind == SourceKind::gob)
        {
            if (sourceIndex >= 0 && static_cast<std::size_t>(sourceIndex) < resources.gobs().size())
            {
                scanGob(resources.gobs()[static_cast<std::size_t>(sourceIndex)]);
            }
        }
        else
        {
            for (const assets::GobArchive& gob : resources.gobs())
            {
                scanGob(gob);
            }
        }
    }

    if (sourceKind == SourceKind::all || sourceKind == SourceKind::diskRoot)
    {
        const std::vector<std::filesystem::path> roots =
            collectDiskRoots(resources, preferredDiskRoot, spec.includeConfiguredDiskRoots);
        if (sourceKind == SourceKind::diskRoot)
        {
            if (sourceIndex >= 0 && static_cast<std::size_t>(sourceIndex) < roots.size())
            {
                scanRoot(roots[static_cast<std::size_t>(sourceIndex)]);
            }
        }
        else
        {
            for (const std::filesystem::path& root : roots)
            {
                scanRoot(root);
            }
        }
    }

    // Keep provider order authoritative here. The picker applies A-Z/Z-A only
    // as a presentation mode; Source order must be able to recover archive,
    // directory, and mount precedence exactly as enumerated.
    auto& cache = sharedResourceListCache();
    if (cache.size() >= 128u && cache.find(cacheKey) == cache.end())
    {
        cache.erase(cache.begin());
    }
    cache[cacheKey] = out;
    return out;
}

assets::JlhTree makeDefaultWavCategoryTree(const std::vector<QString>& resourceNames)
{
    auto uncategorized = std::make_shared<assets::JllList>();
    uncategorized->name = "uncategorized";

    std::unordered_set<std::string> seen{};
    uncategorized->entries.reserve(resourceNames.size());
    for (const QString& rawName : resourceNames)
    {
        const QString name = fileNameOnly(rawName);
        if (name.isEmpty())
        {
            continue;
        }

        const std::string entry = name.toStdString();
        if (seen.insert(sed::text::upperAscii(entry)).second)
        {
            uncategorized->entries.push_back(entry);
        }
    }

    std::sort(
        uncategorized->entries.begin(),
        uncategorized->entries.end(),
        [](const std::string& a, const std::string& b)
        {
            return sed::text::upperAscii(a) < sed::text::upperAscii(b);
        });

    assets::JlhNode root{};
    root.label = "Sounds";
    root.children.push_back(assets::JlhNode{
        "Uncategorized",
        std::move(uncategorized),
        {},
    });

    assets::JlhTree tree{};
    tree.roots.push_back(std::move(root));
    return tree;
}

QImage matImageFromBytes(const std::vector<std::byte>& bytes)
{
    assets::MatFile mat = assets::MatFile::parse(std::vector<std::byte>(bytes.begin(), bytes.end()), nullptr);
    const sed::Rgba8Image img = mat.decodeCelRgba8(0);
    if (img.empty())
    {
        return {};
    }

    QImage out(img.width, img.height, QImage::Format_RGBA8888);
    if (out.isNull())
    {
        return {};
    }
    const std::size_t byteCount = static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height) * 4u;
    std::copy_n(
        reinterpret_cast<const std::byte*>(img.rgba.data()),
        byteCount,
        reinterpret_cast<std::byte*>(out.bits()));
    return out;
}

QImage matImageFromBytes(const std::vector<std::byte>& bytes, const assets::CmpPalette* palette)
{
    assets::MatFile mat = assets::MatFile::parse(std::vector<std::byte>(bytes.begin(), bytes.end()), palette);
    const sed::Rgba8Image img = mat.decodeCelRgba8(0);
    if (img.empty())
    {
        return {};
    }

    QImage out(img.width, img.height, QImage::Format_RGBA8888);
    if (out.isNull())
    {
        return {};
    }
    const std::size_t byteCount = static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height) * 4u;
    std::copy_n(
        reinterpret_cast<const std::byte*>(img.rgba.data()),
        byteCount,
        reinterpret_cast<std::byte*>(out.bits()));
    return out;
}

QPixmap makeMatThumbnail(const QImage& source, const int iconSizePx)
{
    if (source.isNull() || iconSizePx <= 0)
    {
        return {};
    }

    QImage canvas(iconSizePx, iconSizePx, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(QColor(36, 36, 36));

    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Subtle checkerboard background for alpha-heavy MATs.
    constexpr int cell = 8;
    for (int y = 0; y < iconSizePx; y += cell)
    {
        for (int x = 0; x < iconSizePx; x += cell)
        {
            const bool odd = ((x / cell) + (y / cell)) % 2 != 0;
            p.fillRect(QRect(x, y, cell, cell), odd ? QColor(52, 52, 52) : QColor(43, 43, 43));
        }
    }

    const int pad = std::max(2, iconSizePx / 14);
    const QSize targetSize(iconSizePx - 2 * pad, iconSizePx - 2 * pad);
    const QImage scaled = source.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPoint topLeft((iconSizePx - scaled.width()) / 2, (iconSizePx - scaled.height()) / 2);
    p.drawImage(topLeft, scaled);

    p.setPen(QPen(QColor(0, 0, 0, 120), 1.0));
    p.drawRect(canvas.rect().adjusted(0, 0, -1, -1));
    return QPixmap::fromImage(canvas);
}

QPixmap makeImagePreviewPixmap(const QImage& source, const QSize& targetSize)
{
    if (source.isNull())
    {
        return {};
    }

    const int width = std::max(48, targetSize.width());
    const int height = std::max(48, targetSize.height());
    QImage canvas(width, height, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(QColor(18, 20, 28));

    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    constexpr int cell = 10;
    for (int y = 0; y < height; y += cell)
    {
        for (int x = 0; x < width; x += cell)
        {
            const bool odd = ((x / cell) + (y / cell)) % 2 != 0;
            p.fillRect(QRect(x, y, cell, cell), odd ? QColor(44, 46, 50) : QColor(34, 36, 40));
        }
    }

    const int pad = std::max(8, std::min(width, height) / 14);
    const QSize fitSize(std::max(1, width - 2 * pad), std::max(1, height - 2 * pad));
    const QImage scaled = source.scaled(fitSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPoint topLeft((width - scaled.width()) / 2, (height - scaled.height()) / 2);
    p.drawImage(topLeft, scaled);

    p.setPen(QPen(QColor(0, 0, 0, 120), 1.0));
    p.drawRect(canvas.rect().adjusted(0, 0, -1, -1));
    return QPixmap::fromImage(canvas);
}

std::istringstream binaryToStream(const std::vector<std::byte>& bytes)
{
    std::string blob(bytes.size(), '\0');
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        blob[i] = std::to_integer<char>(bytes[i]);
    }
    return std::istringstream(blob, std::ios::binary);
}

std::optional<assets::CmpPalette> tryLoadPreviewPalette(
    const sed::assets::ResourceLocator& resources,
    const std::string_view cmpName)
{
    const std::string cmp = sed::text::trim(cmpName);
    if (cmp.empty())
    {
        return std::nullopt;
    }

    const auto bytes = detail::tryReadPreviewResource(&resources, cmp, ".cmp");
    if (!bytes)
    {
        return std::nullopt;
    }
    return assets::parseCmpPalette(*bytes);
}

std::optional<std::size_t> findTemplateValueIndex(const ThingTemplate& tpl, const std::string_view fieldName)
{
    for (std::size_t i = 0; i < tpl.values.size(); ++i)
    {
        if (sed::text::iequalsAscii(tpl.values[i].name, fieldName))
        {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::string> templateFieldStringValue(const ThingTemplate& tpl, const std::string_view fieldName)
{
    if (const std::optional<std::size_t> index = findTemplateValueIndex(tpl, fieldName))
    {
        const TplValue& value = tpl.values[*index];
        const std::string text = sed::text::trim(
            value.stringValue.empty() ? value.asString() : value.stringValue);
        if (!text.empty())
        {
            return text;
        }
    }
    return std::nullopt;
}

std::optional<std::string> resolvedTemplatePreviewField(
    const ThingTemplate& tpl,
    const assets::Templates* templates,
    const std::string_view tplName,
    const std::string_view fieldName)
{
    if (templates && !tplName.empty())
    {
        if (const TplValue* value = templates->findField(tplName, fieldName))
        {
            const std::string trimmed = sed::text::trim(value->asString());
            if (!trimmed.empty() && !sed::text::iequalsAscii(trimmed, "none"))
            {
                return trimmed;
            }
        }
    }

    if (const std::optional<std::string> direct = templateFieldStringValue(tpl, fieldName))
    {
        const std::string trimmed = sed::text::trim(*direct);
        if (!trimmed.empty() && !sed::text::iequalsAscii(trimmed, "none"))
        {
            return trimmed;
        }
    }

    return std::nullopt;
}

std::optional<ThingBox> resolvedTemplatePreviewBBox(
    const ThingTemplate& tpl,
    const assets::Templates* templates,
    const std::string_view tplName)
{
    if (!tpl.bbox.IsZero())
    {
        return tpl.bbox;
    }

    if (!templates || tplName.empty())
    {
        return std::nullopt;
    }

    std::string current = std::string(tplName);
    for (int depth = 0; depth < 256; ++depth)
    {
        const ThingTemplate* currentTpl = templates->find(current);
        if (!currentTpl)
        {
            break;
        }

        if (!currentTpl->bbox.IsZero())
        {
            return currentTpl->bbox;
        }

        if (currentTpl->parent.empty() || sed::text::iequalsAscii(currentTpl->parent, "none"))
        {
            break;
        }

        current = currentTpl->parent;
    }

    return std::nullopt;
}

std::optional<QImage> loadTemplateSpritePreviewImage(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view spriteName)
{
    return detail::loadSharedPreviewSpriteImage(resources, cmpName, spriteName);
}

QPixmap makeTemplatePreviewPixmap(const QImage& source, const QSize& targetSize)
{
    return detail::makeTemplatePreviewPixmap(source, targetSize);
}

std::vector<QVector3D> modelPreviewWorldVertices(const assets::Model3do& model)
{
    std::vector<QVector3D> vertices{};
    vertices.reserve(256);

    for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
    {
        const assets::Model3doMesh& mesh = model.meshes[meshIndex];
        for (const assets::Model3doVertex& vertex : mesh.vertices)
        {
            vertices.push_back(QVector3D(
                static_cast<float>(vertex.x),
                static_cast<float>(vertex.y),
                static_cast<float>(vertex.z)));
        }
    }

    return vertices;
}

QIcon makeTemplateModelPreviewIcon(
    const assets::Model3do& model,
    const QSize& targetSize,
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const TemplateListModelPreviewMode mode = TemplateListModelPreviewMode::shaded)
{
    if (mode == TemplateListModelPreviewMode::none)
    {
        const int width = std::max(40, targetSize.width());
        const int height = std::max(40, targetSize.height());
        QImage canvas(width, height, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(QColor(18, 20, 28));
        return QIcon(QPixmap::fromImage(canvas));
    }

    return detail::makeTemplateModelPreviewIcon(
        model,
        targetSize,
        resources,
        cmpName,
        mode == TemplateListModelPreviewMode::wireframe);
}

ThingBox spritePreviewBox(const assets::SprFile& spr)
{
    const double halfW = std::max(0.1, static_cast<double>(spr.width) * 0.5);
    const double halfH = std::max(0.1, static_cast<double>(spr.height) * 0.5);
    const double offsetX = static_cast<double>(spr.offsetXYZ[0]);
    const double offsetY = static_cast<double>(spr.offsetXYZ[1]);
    const double offsetZ = static_cast<double>(spr.offsetXYZ[2]);
    const double halfDepth = std::max(0.1, std::max(halfW, halfH) * 0.08);

    ThingBox box{};
    if (spr.type == 2)
    {
        const double radialHalf = std::max(halfW, halfDepth);
        box.Set(
            offsetX - radialHalf,
            offsetY - radialHalf,
            offsetZ - halfH,
            offsetX + radialHalf,
            offsetY + radialHalf,
            offsetZ + halfH);
    }
    else
    {
        box.Set(
            offsetX - halfW,
            offsetY - halfDepth,
            offsetZ - halfH,
            offsetX + halfW,
            offsetY + halfDepth,
            offsetZ + halfH);
    }
    return box;
}

std::optional<ThingBox> inferPickerPreviewBox(const ResourcePickerKind kind, const std::vector<std::byte>& bytes)
{
    try
    {
        if (kind == ResourcePickerKind::spr)
        {
            std::istringstream stream = binaryToStream(bytes);
            const assets::SprFile spr = assets::loadFromSPR(stream);
            return spritePreviewBox(spr);
        }

        if (kind == ResourcePickerKind::model3do)
        {
            std::istringstream stream = binaryToStream(bytes);
            const assets::Model3do model = assets::loadFrom3DO(stream);
            bool haveAny = false;
            ThingBox box{};
            for (const assets::Model3doMesh& mesh : model.meshes)
            {
                for (const assets::Model3doVertex& vertex : mesh.vertices)
                {
                    if (!haveAny)
                    {
                        box.Set(vertex.x, vertex.y, vertex.z, vertex.x, vertex.y, vertex.z);
                        haveAny = true;
                        continue;
                    }

                    box.x1 = std::min(box.x1, static_cast<double>(vertex.x));
                    box.y1 = std::min(box.y1, static_cast<double>(vertex.y));
                    box.z1 = std::min(box.z1, static_cast<double>(vertex.z));
                    box.x2 = std::max(box.x2, static_cast<double>(vertex.x));
                    box.y2 = std::max(box.y2, static_cast<double>(vertex.y));
                    box.z2 = std::max(box.z2, static_cast<double>(vertex.z));
                }
            }
            if (haveAny)
            {
                return box;
            }
        }
    }
    catch (const std::exception&)
    {
    }

    return std::nullopt;
}

std::optional<ThingBox> inferTemplatePreviewBox(
    const ThingTemplate& tpl,
    const assets::Templates* templates,
    const std::string_view tplName,
    const assets::ResourceLocator* resources)
{
    if (!resources)
    {
        return resolvedTemplatePreviewBBox(tpl, templates, tplName);
    }

    if (const std::optional<std::string> spriteName = resolvedTemplatePreviewField(tpl, templates, tplName, "sprite"))
    {
        if (const std::shared_ptr<const assets::SprFile> sprite =
                detail::loadSharedPreviewSprite(resources, *spriteName))
        {
            return spritePreviewBox(*sprite);
        }
    }

    std::optional<std::string> modelName = resolvedTemplatePreviewField(tpl, templates, tplName, "model3d");
    if (!modelName)
    {
        modelName = resolvedTemplatePreviewField(tpl, templates, tplName, "model3do");
    }
    if (modelName)
    {
        if (const std::optional<ThingBox> box = detail::loadSharedPreviewModelBox(resources, *modelName))
        {
            return *box;
        }
    }

    return resolvedTemplatePreviewBBox(tpl, templates, tplName);
}

ThingTemplate makePickerPreviewTemplate(const ResourcePickerKind kind, const std::string_view resourceName)
{
    ThingTemplate tpl{};
    tpl.name = "__picker_preview__";
    tpl.parent = "none";

    if (kind == ResourcePickerKind::spr)
    {
        TplValue sprite{};
        sprite.name = "sprite";
        sprite.type = ValueType::str;
        sprite.adjType = AdjType::spr;
        sprite.stringValue = std::string(resourceName);
        tpl.values.push_back(std::move(sprite));

        TplValue type{};
        type.name = "type";
        type.type = ValueType::str;
        type.stringValue = "sprite";
        tpl.values.push_back(std::move(type));
    }
    else
    {
        TplValue model{};
        model.name = "model3do";
        model.type = ValueType::str;
        model.adjType = AdjType::model3do;
        model.stringValue = std::string(resourceName);
        tpl.values.push_back(std::move(model));
    }

    return tpl;
}

class ThingPreviewWidget final : public QWidget
{
public:
    ThingPreviewWidget(
        std::shared_ptr<const sed::assets::ResourceLocator> resources,
        const ResourcePickerOptions& options,
        QWidget* parent = nullptr)
        : QWidget(parent)
        , resources_(std::move(resources))
        , options_(options)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        stack_ = new QStackedWidget(this);
        layout->addWidget(stack_);

        emptyLabel_ = new QLabel(stack_);
        emptyLabel_->setAlignment(Qt::AlignCenter);
        emptyLabel_->setWordWrap(true);
        emptyLabel_->setStyleSheet(
            "QLabel {"
            " color:#9aa7bb;"
            " border:1px solid rgba(120,135,160,0.35);"
            " border-radius:10px;"
            " background:rgba(6,10,18,0.72);"
            " padding:18px;"
            " }");
        stack_->addWidget(emptyLabel_);

        spriteLabel_ = new QLabel(stack_);
        spriteLabel_->setAlignment(Qt::AlignCenter);
        spriteLabel_->setStyleSheet(
            "QLabel {"
            " border:1px solid rgba(120,135,160,0.35);"
            " border-radius:10px;"
            " background:rgba(6,10,18,0.72);"
            " padding:10px;"
            " }");
        spriteLabel_->installEventFilter(this);
        stack_->addWidget(spriteLabel_);

        spriteAnimBtn_ = new QToolButton(spriteLabel_);
        spriteAnimBtn_->setAutoRaise(true);
        spriteAnimBtn_->setToolTip("Play / Stop animation");
        spriteAnimBtn_->setFixedSize(26, 26);
        spriteAnimBtn_->setStyleSheet(
            "QToolButton {"
            " background:rgba(28,30,36,200);"
            " border:1px solid rgba(160,170,190,0.45);"
            " border-radius:4px;"
            " color:#dce0e8;"
            " font-weight:bold;"
            " font-size:14px;"
            " }"
            "QToolButton:hover { background:rgba(50,54,64,220); }");
        spriteAnimBtn_->hide();
        connect(spriteAnimBtn_, &QToolButton::clicked, this, [this]()
            {
                if (spriteAnimTimer_ && spriteAnimTimer_->isActive())
                {
                    stopSpriteAnimation();
                }
                else
                {
                    startSpriteAnimation();
                }
            });

        spriteAnimSpeedSlider_ = new QSlider(Qt::Horizontal, spriteLabel_);
        spriteAnimSpeedSlider_->setRange(1, 20);
        spriteAnimSpeedSlider_->setValue(10);
        spriteAnimSpeedSlider_->setSingleStep(1);
        spriteAnimSpeedSlider_->setPageStep(2);
        spriteAnimSpeedSlider_->setFixedSize(92, 18);
        spriteAnimSpeedSlider_->setStyleSheet(
            "QSlider::groove:horizontal {"
            " background:rgba(28,30,36,200);"
            " border:1px solid rgba(160,170,190,0.35);"
            " height:6px;"
            " border-radius:3px;"
            " }"
            "QSlider::sub-page:horizontal {"
            " background:rgba(98, 191, 214, 220);"
            " border-radius:3px;"
            " }"
            "QSlider::handle:horizontal {"
            " background:#dce0e8;"
            " width:10px;"
            " margin:-4px 0;"
            " border-radius:5px;"
            " }");
        spriteAnimSpeedSlider_->hide();
        connect(spriteAnimSpeedSlider_, &QSlider::valueChanged, this, [this](const int)
            {
                updateSpriteAnimationSpeed();
            });

        spriteAnimTimer_ = new QTimer(this);
        updateSpriteAnimationSpeed();
        connect(spriteAnimTimer_, &QTimer::timeout, this, [this]()
            {
                if (spriteAnimFrames_.empty())
                {
                    spriteAnimTimer_->stop();
                    return;
                }
                spriteAnimIndex_ = (spriteAnimIndex_ + 1) % static_cast<int>(spriteAnimFrames_.size());
                currentSpriteImage_ = spriteAnimFrames_[static_cast<std::size_t>(spriteAnimIndex_)];
                updateSpritePreviewPixmap();
            });

        const AssetPreviewSpinSettings spinSettings = loadAssetPreviewSpinSettings();
        autoRotateEnabled_ = spinSettings.enabled;
        autoRotateSpeedDegPerSec_ = spinSettings.speedDegPerSec;

        spinTimer_ = new QTimer(this);
        spinTimer_->setInterval(detail::kAssetPreviewSpinIntervalMs);
        spinElapsed_.start();
        connect(spinTimer_, &QTimer::timeout, this, [this]()
            {
                if (!view_ || stack_->currentWidget() != view_ || !isVisible() || !view_->isVisible())
                {
                    spinElapsed_.restart();
                    return;
                }
                if (previewMouseInteractionActive_)
                {
                    spinElapsed_.restart();
                    return;
                }
                if (detail::sharedThumbnailRenderBusy())
                {
                    return;
                }
                if (!autoRotateEnabled_ || autoRotateSpeedDegPerSec_ <= 0.0f)
                {
                    spinElapsed_.restart();
                    return;
                }
                const std::int64_t elapsedMs = spinElapsed_.isValid()
                    ? spinElapsed_.restart()
                    : spinTimer_->interval();
                const float stepDeg = detail::assetPreviewSpinStepDegrees(
                    autoRotateSpeedDegPerSec_, elapsedMs);
                view_->orbitRotate(-stepDeg, 0.0f);
            });
        spinTimer_->start();

        refitTimer_ = new QTimer(this);
        refitTimer_->setSingleShot(true);
        refitTimer_->setInterval(90);
        connect(refitTimer_, &QTimer::timeout, this, [this]()
            {
                if (!view_ || !currentPreviewBox_ || !stack_ || stack_->currentWidget() != view_)
                {
                    return;
                }
                fitToBox(*currentPreviewBox_, false);
            });

        showMessage("No previewable resource selected.");
    }

    void setPreviewProjectionType(const app::ViewportCamera::ProjectionType type)
    {
        projectionType_ = type;
        applyViewPreviewMode();
        if (currentPreviewBox_ && view_ && stack_ && stack_->currentWidget() == view_)
        {
            fitToBox(*currentPreviewBox_, false);
        }
        if (view_)
        {
            view_->update();
        }
    }

    void setResourceArchiveReloadCallback(std::function<bool()> callback)
    {
        resourceArchiveReloadCallback_ = std::move(callback);
        if ( view_ )
        {
            view_->setResourceArchiveReloadCallback(
                resourceArchiveReloadCallback_);
        }
    }

    void setResourceArchiveChangedCallback(std::function<void()> callback)
    {
        resourceArchiveChangedCallback_ = std::move(callback);
    }

    void setPreviewRenderMode(const int mode)
    {
        renderMode_ = std::clamp(mode, -2, 1);
        applyViewPreviewMode();
        if (view_)
        {
            view_->update();
        }
    }

    void fitPreview(const bool resetOrbit = false)
    {
        if (currentPreviewBox_)
        {
            fitToBox(*currentPreviewBox_, resetOrbit);
        }
    }

    bool hasInteractiveGlPreview() const noexcept
    {
        return stack_ && view_ && stack_->currentWidget() == view_ && currentPreviewBox_.has_value();
    }

    void prepareInteractiveGlHost()
    {
        // QOpenGLWidget changes the top-level window's surface type. Create
        // the empty host before the modal is ever shown so Qt can choose the
        // correct native surface on its first mapping. Model bytes, scene
        // state, GPU uploads, and the visible preview remain deferred until a
        // valid selection is ready.
        ensureView();
    }

    void reloadCachedResources()
    {
        if (view_)
        {
            view_->reloadCachedResources();
        }
    }

    void invalidateResourceCaches()
    {
        previewPalette_.reset();
        previewPaletteResolved_ = false;
        currentSpriteImage_.reset();
        currentPreviewBox_.reset();
        resetSpriteAnimationState();
        reloadCachedResources();
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    std::vector<QImage> animationFramesForTest() const
    {
        return spriteAnimFrames_;
    }

    QImage previewFrameForTest()
    {
        return view_ ? view_->grabFramebuffer() : QImage{};
    }

    bool texturesEnabledForTest() const noexcept
    {
        return view_ && view_->showTextures();
    }

    bool solidEnabledForTest() const noexcept
    {
        return view_ && view_->showSolid();
    }

    int renderModeForTest() const noexcept
    {
        return renderMode_;
    }

    void freezeAutoRotationForTest()
    {
        autoRotateEnabled_ = false;
        autoRotateSpeedDegPerSec_ = 0.0f;
        if ( spinTimer_ )
        {
            spinTimer_->stop();
        }
        spinElapsed_.invalidate();
    }

    detail::PreviewResourceDiskWatcher* resourceDiskWatcherForTest() const noexcept
    {
        return view_ ? view_->resourceDiskWatcherForTest() : nullptr;
    }
#endif

    void setResourceSnapshot(
        std::shared_ptr<const sed::assets::ResourceLocator> resources)
    {
        if (!resources || resources_ == resources)
        {
            return;
        }

        resources_ = std::move(resources);
        if (view_)
        {
            view_->setResourceLocator(resources_.get());
        }
        invalidateResourceCaches();
    }

    void clearPreview(const QString& message = "No previewable resource selected.")
    {
        templates_.clear();
        level_.clear();
        currentPreviewBox_.reset();
        currentSpriteImage_ = {};
        spriteAnimFrames_.clear();
        spriteAnimIndex_ = 0;
        resetSpriteAnimationState();
        if (spriteLabel_)
        {
            spriteLabel_->setPixmap(QPixmap{});
            spriteLabel_->setText({});
        }
        if (view_)
        {
            view_->setTemplateDatabase(nullptr);
            view_->setLevel(nullptr, GlView::SetLevelMode::keepView);
        }
        showMessage(message);
    }

    void setPreviewResource(
        const ResourcePickerKind kind,
        const QString& name,
        const std::optional<std::vector<std::byte>>& bytes)
    {
        if (name.trimmed().isEmpty())
        {
            clearPreview("No previewable resource selected.");
            return;
        }

        if (kind != ResourcePickerKind::model3do && kind != ResourcePickerKind::spr && !bytes)
        {
            qCWarning(logResources).noquote()
                << "Resource preview load failed:" << name
                << "could not be read";
            clearPreview(QString("Couldn't load %1.").arg(name));
            return;
        }

        currentSpriteImage_ = {};
        currentPreviewBox_.reset();
        resetSpriteAnimationState();
        if (spriteLabel_)
        {
            spriteLabel_->setPixmap(QPixmap{});
            spriteLabel_->setText({});
        }

        if (kind == ResourcePickerKind::spr)
        {
            currentSpriteImage_ =
                detail::loadSharedPreviewSpriteImage(resources_.get(), options_.previewCmp, name.toStdString());
            if (currentSpriteImage_ && !currentSpriteImage_->isNull())
            {
                if (view_)
                {
                    view_->setTemplateDatabase(nullptr);
                    view_->setLevel(nullptr, GlView::SetLevelMode::keepView);
                }
                updateSpritePreviewPixmap();
                stack_->setCurrentWidget(spriteLabel_);
                setupSpriteAnimForCurrentSprite(name.toStdString());
                return;
            }

            qCWarning(logResources).noquote()
                << "SPR preview failed:" << name;
            clearPreview(QString("Couldn't preview %1.").arg(name));
            return;
        }

        std::optional<ThingBox> box{};
        if (kind == ResourcePickerKind::model3do)
        {
            box = detail::loadSharedPreviewModelBox(resources_.get(), name.toStdString());
        }
        else
        {
            box = inferPickerPreviewBox(kind, *bytes);
        }
        if (!box)
        {
            qCWarning(logResources).noquote()
                << "Resource preview geometry failed:" << name;
            clearPreview(QString("Couldn't preview %1.").arg(name));
            return;
        }

        ThingTemplate previewTemplate = makePickerPreviewTemplate(kind, name.toStdString());
        previewTemplate.bbox = *box;
        currentPreviewBox_ = *box;

        ensureView();

        templates_.clear();
        (void)templates_.addOrUpdate(previewTemplate, true);

        level_.clear();
        level_.kind = options_.previewProjectType;
        level_.masterCMP = options_.previewCmp;
        level_.ppunit = 320.0;
        level_.templates.clear();
        level_.templates.push_back(previewTemplate);
        level_.sectors.push_back(Sector{});
        level_.sectors.back().layer = 0;
        level_.sectors.back().ambient = ColorWhite;

        Thing previewThing{};
        previewThing.name = previewTemplate.name;
        previewThing.templateName = previewTemplate.name;
        previewThing.sector = 0;
        previewThing.bbox = *box;
        level_.things.push_back(previewThing);

        view_->setResourceLocator(resources_.get());
        if (kind == ResourcePickerKind::model3do)
        {
            detail::primeSharedModelPreviewAssets(
                *view_, resources_.get(), options_.previewCmp, name.toStdString());
        }
        view_->setTemplateDatabase(&templates_);
        view_->setRenderPipeline(GlView::RenderPipeline::Editor);
        view_->resetPreviewViewportState();
        view_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
        view_->setShowSolid(true);
        view_->setShowTextures(true);
        view_->setShowThings(true);
        view_->setShowEditorCubes(false);
        view_->setShowPreviewCubes(false);
        view_->setThingViewMode(GlView::ThingViewMode::Shaded);
        view_->setLightingMode(GlView::LightingMode::Gouraud);
        view_->setLevel(&level_, GlView::SetLevelMode::resetView);
        applyViewPreviewMode();
        stack_->setCurrentWidget(view_);
        fitToBox(*box);
    }

    void setPreviewTemplate(const assets::Templates& database, const ThingTemplate& tpl)
    {
        const QString tplName = QString::fromStdString(tpl.name).trimmed();
        if (tplName.isEmpty())
        {
            clearPreview("No previewable template selected.");
            return;
        }

        currentSpriteImage_ = {};
        currentPreviewBox_.reset();
        resetSpriteAnimationState();
        if (spriteLabel_)
        {
            spriteLabel_->setPixmap(QPixmap{});
            spriteLabel_->setText({});
        }

        if (const std::optional<std::string> spriteName = resolvedTemplatePreviewField(tpl, &database, tpl.name, "sprite"))
        {
            currentSpriteImage_ = loadTemplateSpritePreviewImage(resources_.get(), options_.previewCmp, *spriteName);
            if (currentSpriteImage_ && !currentSpriteImage_->isNull())
            {
                if (view_)
                {
                    view_->setTemplateDatabase(nullptr);
                    view_->setLevel(nullptr, GlView::SetLevelMode::keepView);
                }
                updateSpritePreviewPixmap();
                stack_->setCurrentWidget(spriteLabel_);
                setupSpriteAnimForCurrentSprite(*spriteName);
                return;
            }
        }

        std::optional<std::string> modelName = resolvedTemplatePreviewField(tpl, &database, tpl.name, "model3d");
        if (!modelName)
        {
            modelName = resolvedTemplatePreviewField(tpl, &database, tpl.name, "model3do");
        }
        const std::optional<ThingBox> box = modelName
            ? detail::loadSharedPreviewModelBox(resources_.get(), *modelName)
            : std::nullopt;
        if (!box)
        {
            qCWarning(logResources).noquote()
                << "Template preview failed:" << tplName
                << (modelName
                    ? QStringLiteral("model %1 could not be prepared")
                        .arg(QString::fromStdString(*modelName))
                    : QStringLiteral("has no previewable model or sprite"));
            clearPreview(modelName
                    ? QString("Couldn't preview model %1.").arg(QString::fromStdString(*modelName))
                    : QString("Couldn't preview %1.").arg(tplName));
            return;
        }
        currentPreviewBox_ = *box;

        ensureView();

        templates_.clear();
        for (std::size_t i = 0; i < database.size(); ++i)
        {
            if (const ThingTemplate* item = database.get(i))
            {
                try
                {
                    (void)templates_.addOrUpdate(*item, true);
                }
                catch (const std::exception&)
                {
                }
            }
        }

        level_.clear();
        level_.kind = options_.previewProjectType;
        level_.masterCMP = options_.previewCmp;
        level_.ppunit = 320.0;
        level_.templates.clear();
        for (std::size_t i = 0; i < templates_.size(); ++i)
        {
            if (const ThingTemplate* item = templates_.get(i))
            {
                level_.templates.push_back(*item);
            }
        }
        level_.sectors.push_back(Sector{});
        level_.sectors.back().layer = 0;
        level_.sectors.back().ambient = ColorWhite;

        Thing previewThing{};
        previewThing.name = tpl.name;
        previewThing.templateName = tpl.name;
        previewThing.sector = 0;
        previewThing.bbox = *box;
        level_.things.push_back(previewThing);

        view_->setResourceLocator(resources_.get());
        detail::primeSharedModelPreviewAssets(
            *view_, resources_.get(), options_.previewCmp, *modelName);
        view_->setTemplateDatabase(&templates_);
        view_->setRenderPipeline(GlView::RenderPipeline::Editor);
        view_->resetPreviewViewportState();
        view_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
        view_->setShowSolid(true);
        view_->setShowTextures(true);
        view_->setShowThings(true);
        view_->setShowEditorCubes(false);
        view_->setShowPreviewCubes(false);
        view_->setThingViewMode(GlView::ThingViewMode::Shaded);
        view_->setLightingMode(GlView::LightingMode::Gouraud);
        view_->setLevel(&level_, GlView::SetLevelMode::resetView);
        applyViewPreviewMode();
        stack_->setCurrentWidget(view_);
        fitToBox(*box);
    }

private:
    void ensureView()
    {
        if (view_ || !stack_)
        {
            return;
        }

        // Raw 3DO and template pickers call this before their first Show so
        // Qt chooses an OpenGL-capable top-level surface on the initial native
        // mapping. Other preview kinds may still construct the host lazily.
        // This creates only the empty viewport widget; model loading, scene
        // binding, GPU uploads, and visible interaction stay selection-driven.
        view_ = new GlView(stack_);
        view_->setObjectName("resourcePickerThingPreview");
        view_->setResourceLocator(resources_.get());
        view_->setResourceArchiveReloadCallback(
            resourceArchiveReloadCallback_);
        QObject::connect(
            view_,
            &GlView::resourceArchivesChanged,
            this,
            [this]()
            {
                if ( resourceArchiveChangedCallback_ )
                {
                    resourceArchiveChangedCallback_();
                }
            });
        view_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
        view_->setShowGrid(false);
        view_->setShowWireframe(false);
        view_->setShowVertices(false);
        view_->setShowSolid(true);
        view_->setShowTextures(true);
        view_->setShowThings(true);
        view_->setShowEditorCubes(false);
        view_->setShowPreviewCubes(false);
        view_->setShowThingGizmos(false);
        view_->setThingViewMode(GlView::ThingViewMode::Shaded);
        view_->setLightingMode(GlView::LightingMode::Gouraud);
        view_->setCameraProjectionType(projectionType_);
        view_->setViewportPickingEnabled(false);
        view_->setHoverPickEnabled(false);
        view_->setPreviewInteractionEnabled(true);
        view_->installEventFilter(this);
        stack_->addWidget(view_);
        applyViewPreviewMode();
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (spriteLabel_ && watched == spriteLabel_ && event && event->type() == QEvent::Resize)
        {
            updateSpritePreviewPixmap();
            repositionSpriteAnimControls();
        }
        if (view_ && watched == view_ && event && event->type() == QEvent::Resize)
        {
            if (refitTimer_ && currentPreviewBox_ && stack_ && stack_->currentWidget() == view_)
            {
                refitTimer_->start();
            }
        }
        if (view_ && watched == view_ && event)
        {
            switch (event->type())
            {
                case QEvent::MouseButtonPress:
                {
                    const auto* mouseEvent = static_cast<QMouseEvent*>(event);
                    if (mouseEvent->button() == Qt::LeftButton
                        || mouseEvent->button() == Qt::MiddleButton
                        || mouseEvent->button() == Qt::RightButton)
                    {
                        previewMouseInteractionActive_ = true;
                    }
                    break;
                }
                case QEvent::MouseButtonRelease:
                {
                    const auto* mouseEvent = static_cast<QMouseEvent*>(event);
                    if (mouseEvent->buttons() == Qt::NoButton)
                    {
                        previewMouseInteractionActive_ = false;
                    }
                    break;
                }
                case QEvent::Hide:
                case QEvent::Leave:
                case QEvent::FocusOut:
                    previewMouseInteractionActive_ = false;
                    break;
                default:
                    break;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void showMessage(const QString& text)
    {
        if (emptyLabel_)
        {
            emptyLabel_->setText(text);
        }
        if (stack_)
        {
            stack_->setCurrentWidget(emptyLabel_);
        }
    }

    void applyViewPreviewMode()
    {
        if (!view_)
        {
            return;
        }

        view_->setCameraProjectionType(projectionType_);
        detail::applyThingAssetPreviewRenderMode(*view_, renderMode_);
    }

    const assets::CmpPalette* previewPalette()
    {
        if (previewPaletteResolved_)
        {
            return previewPalette_ ? &*previewPalette_ : nullptr;
        }

        previewPaletteResolved_ = true;
        previewPalette_ = tryLoadPreviewPalette(*resources_, options_.previewCmp);
        return previewPalette_ ? &*previewPalette_ : nullptr;
    }

    void updateSpritePreviewPixmap()
    {
        if (!spriteLabel_)
        {
            return;
        }
        if (!currentSpriteImage_ || currentSpriteImage_->isNull())
        {
            spriteLabel_->setPixmap(QPixmap{});
            return;
        }

        spriteLabel_->setPixmap(makeImagePreviewPixmap(*currentSpriteImage_, spriteLabel_->contentsRect().size()));
    }

    void repositionSpriteAnimControls()
    {
        if (!spriteAnimBtn_ || !spriteLabel_ || !spriteAnimSpeedSlider_)
        {
            return;
        }
        const int margin = 8;
        const int buttonX = spriteLabel_->width() - spriteAnimBtn_->width() - margin;
        const int buttonY = spriteLabel_->height() - spriteAnimBtn_->height() - margin;
        spriteAnimBtn_->move(buttonX, buttonY);
        const int sliderY = buttonY + std::max(0, (spriteAnimBtn_->height() - spriteAnimSpeedSlider_->height()) / 2);
        const int sliderX = buttonX - spriteAnimSpeedSlider_->width() - 6;
        spriteAnimSpeedSlider_->move(std::max(margin, sliderX), sliderY);
    }

    void updateSpriteAnimationSpeed()
    {
        const int fps = spriteAnimSpeedSlider_ ? std::max(1, spriteAnimSpeedSlider_->value()) : 10;
        const int intervalMs = std::max(20, static_cast<int>(std::lround(1000.0 / static_cast<double>(fps))));
        if (spriteAnimTimer_)
        {
            spriteAnimTimer_->setInterval(intervalMs);
        }
        if (spriteAnimSpeedSlider_)
        {
            spriteAnimSpeedSlider_->setToolTip(QString("Animation speed: %1 fps").arg(fps));
        }
    }

    void loadSpriteAnimFrames(const std::string_view spriteName)
    {
        spriteAnimFrames_.clear();
        spriteAnimIndex_ = 0;

        const std::shared_ptr<const assets::SprFile> sprite =
            detail::loadSharedPreviewSprite(resources_.get(), spriteName);
        if (!sprite)
        {
            return;
        }

        try
        {
            const auto matBytes = resources_->tryReadBinaryWithExt(sprite->material, ".mat");
            if (!matBytes)
            {
                return;
            }

            const assets::CmpPalette* palette = previewPalette();
            assets::MatFile mat = assets::MatFile::parse(
                std::vector<std::byte>(matBytes->begin(), matBytes->end()),
                palette);

            const std::int32_t count = mat.celCount();
            if (count <= 1)
            {
                return;
            }

            spriteAnimFrames_.reserve(static_cast<std::size_t>(count));
            for (std::int32_t i = 0; i < count; ++i)
            {
                QImage img = detail::qImageFromRgba8(mat.decodeCelRgba8(i));
                if (img.isNull())
                {
                    spriteAnimFrames_.clear();
                    return;
                }
                spriteAnimFrames_.push_back(std::move(img));
            }
        }
        catch (const std::exception&)
        {
            spriteAnimFrames_.clear();
        }
    }

    void startSpriteAnimation()
    {
        if (spriteAnimFrames_.size() <= 1 || !spriteAnimTimer_)
        {
            return;
        }
        updateSpriteAnimationSpeed();
        spriteAnimTimer_->start();
        if (spriteAnimBtn_)
        {
            spriteAnimBtn_->setText(QStringLiteral("\u25A0")); // stop square
        }
    }

    void stopSpriteAnimation()
    {
        if (spriteAnimTimer_)
        {
            spriteAnimTimer_->stop();
        }
        spriteAnimIndex_ = 0;
        if (!spriteAnimFrames_.empty())
        {
            currentSpriteImage_ = spriteAnimFrames_.front();
            updateSpritePreviewPixmap();
        }
        if (spriteAnimBtn_)
        {
            spriteAnimBtn_->setText(QStringLiteral("\u25B6")); // play triangle
        }
    }

    void resetSpriteAnimationState()
    {
        if (spriteAnimTimer_)
        {
            spriteAnimTimer_->stop();
        }
        spriteAnimFrames_.clear();
        spriteAnimIndex_ = 0;
        if (spriteAnimBtn_)
        {
            spriteAnimBtn_->setText(QStringLiteral("\u25B6"));
            spriteAnimBtn_->hide();
        }
        if (spriteAnimSpeedSlider_)
        {
            spriteAnimSpeedSlider_->hide();
        }
    }

    void setupSpriteAnimForCurrentSprite(const std::string_view spriteName)
    {
        resetSpriteAnimationState();

        loadSpriteAnimFrames(spriteName);

        if (spriteAnimFrames_.size() > 1 && spriteAnimBtn_ && spriteAnimSpeedSlider_)
        {
            spriteAnimBtn_->setText(QStringLiteral("\u25B6")); // play triangle
            spriteAnimBtn_->show();
            spriteAnimSpeedSlider_->show();
            repositionSpriteAnimControls();
            startSpriteAnimation();
        }
    }

    void fitToBox(const ThingBox& box, const bool resetOrbit = true)
    {
        if (!view_)
        {
            return;
        }

        QVector3D minv(
            static_cast<float>(std::min(box.x1, box.x2)),
            static_cast<float>(std::min(box.y1, box.y2)),
            static_cast<float>(std::min(box.z1, box.z2)));
        QVector3D maxv(
            static_cast<float>(std::max(box.x1, box.x2)),
            static_cast<float>(std::max(box.y1, box.y2)),
            static_cast<float>(std::max(box.z1, box.z2)));
        if ((maxv - minv).lengthSquared() <= 1.0e-6f)
        {
            maxv += QVector3D(0.5f, 0.5f, 0.5f);
            minv -= QVector3D(0.5f, 0.5f, 0.5f);
        }

        const QVector3D center = (minv + maxv) * 0.5f;
        const QVector3D halfExtents = (maxv - minv) * 0.5f * kAssetPreviewFitPadding;
        minv = center - halfExtents;
        maxv = center + halfExtents;

        if (resetOrbit)
        {
            view_->setOrbitViewImmediate(
                kAssetPreviewDefaultYawDeg,
                kAssetPreviewDefaultPitchDeg,
                kAssetPreviewDefaultRollDeg);
        }
        view_->frameWorldBounds(minv, maxv, false);
        view_->tightenOrthoClipToWorldBounds(minv, maxv);
        QTimer::singleShot(0, view_, [view = view_, minv, maxv, resetOrbit]()
            {
                if (view)
                {
                    if (resetOrbit)
                    {
                        view->setOrbitViewImmediate(
                            kAssetPreviewDefaultYawDeg,
                            kAssetPreviewDefaultPitchDeg,
                            kAssetPreviewDefaultRollDeg);
                    }
                    view->frameWorldBounds(minv, maxv, false);
                    view->tightenOrthoClipToWorldBounds(minv, maxv);
                }
            });
    }

    std::shared_ptr<const sed::assets::ResourceLocator> resources_{};
    ResourcePickerOptions options_{};
    QStackedWidget* stack_{};
    QLabel* emptyLabel_{};
    QLabel* spriteLabel_{};
    QToolButton* spriteAnimBtn_{};
    QSlider* spriteAnimSpeedSlider_{};
    QTimer* spriteAnimTimer_{};
    std::vector<QImage> spriteAnimFrames_{};
    int spriteAnimIndex_{};
    GlView* view_{};
    QTimer* spinTimer_{};
    QElapsedTimer spinElapsed_{};
    QTimer* refitTimer_{};
    assets::Templates templates_{};
    Level level_{};
    std::optional<assets::CmpPalette> previewPalette_{};
    bool previewPaletteResolved_{};
    std::optional<QImage> currentSpriteImage_{};
    std::optional<ThingBox> currentPreviewBox_{};
    std::function<bool()> resourceArchiveReloadCallback_{};
    std::function<void()> resourceArchiveChangedCallback_{};
    bool autoRotateEnabled_{ true };
    bool previewMouseInteractionActive_{};
    float autoRotateSpeedDegPerSec_{ 72.0f };
    app::ViewportCamera::ProjectionType projectionType_{ app::ViewportCamera::ProjectionType::Orthographic };
    int renderMode_{ 1 };
};

class ResourcePickerDialog final : public QDialog
{
public:
    ResourcePickerDialog(
        QWidget* parent,
        std::shared_ptr<const sed::assets::ResourceLocator> resources,
        const ResourcePickerOptions& options)
        : QDialog(parent)
        , resources_(std::move(resources))
        , thumbnailResources_(resources_)
        , options_(options)
        , spec_(specForKind(options.kind))
    {
        if (!resources_)
        {
            throw std::invalid_argument("Resource picker requires an immutable ResourceLocator snapshot");
        }

        ensureGlobalComponentStyles();
        QPalette pickerPalette = palette();
        pickerPalette.setColor(QPalette::Window, QColor(43, 43, 43));
        pickerPalette.setColor(QPalette::WindowText, QColor(238, 238, 238));
        pickerPalette.setColor(QPalette::Base, QColor(32, 32, 32));
        pickerPalette.setColor(QPalette::AlternateBase, QColor(36, 36, 36));
        pickerPalette.setColor(QPalette::Text, QColor(238, 238, 238));
        setPalette(pickerPalette);
        setAutoFillBackground(true);
        setAttribute(Qt::WA_StyledBackground, true);
        setProperty("resourcePickerTemplatePreviewBatchSerialForTest",
            QVariant::fromValue<qulonglong>(0));

        const bool hasCategoryTree = categoryFileNameForPickerKind(options_.kind, options_.previewProjectType).has_value();
        setWindowTitle(options_.title.empty()
                           ? QString::fromStdString(resourcePickerTitle(options_.kind))
                           : QString::fromStdString(options_.title));
        setModal(true);
        resize(1420, 760);
        setMinimumSize(980, 620);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(8);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        root->addWidget(splitter, 1);

        auto* leftPane = new QWidget(splitter);
        auto* leftLayout = new QVBoxLayout(leftPane);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(6);

        auto* sourcesHeader = new QWidget(leftPane);
        auto* sourcesHeaderLayout = new QHBoxLayout(sourcesHeader);
        sourcesHeaderLayout->setContentsMargins(0, 0, 0, 0);
        sourcesHeaderLayout->setSpacing(6);

        auto* sourcesLabel = new QLabel(QStringLiteral("Sections"), sourcesHeader);
        sourcesLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
        sourcesHeaderLayout->addWidget(sourcesLabel);
        sourcesHeaderLayout->addStretch(1);

        editCategoriesButton_ = new QToolButton(sourcesHeader);
        editCategoriesButton_->setText(QStringLiteral("Edit..."));
        editCategoriesButton_->setAutoRaise(true);
        editCategoriesButton_->setToolTip(QStringLiteral("Edit the category files used by this picker"));
        editCategoriesButton_->hide();
        sourcesHeaderLayout->addWidget(editCategoriesButton_);
        leftLayout->addWidget(sourcesHeader);

        sources_ = new detail::CategoryTreeWidget(leftPane);
        sources_->setHeaderHidden(true);
        sources_->setRootIsDecorated(hasCategoryTree);
        sources_->setUniformRowHeights(true);
        sources_->setSelectionMode(QAbstractItemView::SingleSelection);
        sources_->setSelectionBehavior(QAbstractItemView::SelectRows);
        sources_->setIndentation(hasCategoryTree ? 16 : 0);
        sources_->setContextMenuPolicy(Qt::CustomContextMenu);
        sources_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        if (hasCategoryTree)
        {
            sources_->setToolTip(QStringLiteral(
                "Right-click to add, nest, rename, or delete categories. "
                "Drag resources onto a category to assign them."));
        }
        leftLayout->addWidget(sources_, 1);

        auto* rightPane = new QWidget(splitter);
        auto* rightLayout = new QVBoxLayout(rightPane);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(6);

        auto* previewHeader = new QWidget(rightPane);
        auto* previewHeaderLayout = new QHBoxLayout(previewHeader);
        previewHeaderLayout->setContentsMargins(0, 0, 0, 0);
        previewHeaderLayout->setSpacing(8);
        auto* previewLabel = new QLabel("Preview", previewHeader);
        previewLabel->setStyleSheet("font-weight:600;");
        previewHeaderLayout->addWidget(previewLabel);
        previewHeaderLayout->addStretch(1);

        const bool hasSoundPreviewControls = options_.kind == ResourcePickerKind::wav;
        auto* soundPreviewControls = new QWidget(previewHeader);
        soundPreviewControls->setObjectName(QStringLiteral("resourcePickerSoundPreviewControls"));
        auto* soundPreviewControlsLayout = new QHBoxLayout(soundPreviewControls);
        soundPreviewControlsLayout->setContentsMargins(0, 0, 0, 0);
        soundPreviewControlsLayout->setSpacing(6);

        previewReplayButton_ = new QToolButton(soundPreviewControls);
        previewReplayButton_->setObjectName(QStringLiteral("resourcePickerReplayButton"));
        previewReplayButton_->setIcon(makeViewportIcon(ViewportIcon::gamePreview));
        previewReplayButton_->setIconSize(QSize(16, 16));
        previewReplayButton_->setText(QStringLiteral("Replay"));
        previewReplayButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        previewReplayButton_->setToolTip(QStringLiteral("Play the selected sound again"));
        previewReplayButton_->setEnabled(false);
        soundPreviewControlsLayout->addWidget(previewReplayButton_);

        previewLoopButton_ = new QToolButton(soundPreviewControls);
        previewLoopButton_->setObjectName(QStringLiteral("resourcePickerLoopButton"));
        previewLoopButton_->setIcon(makeViewportIcon(ViewportIcon::actionReload));
        previewLoopButton_->setIconSize(QSize(16, 16));
        previewLoopButton_->setText(QStringLiteral("Loop"));
        previewLoopButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        previewLoopButton_->setToolTip(QStringLiteral("Continuously loop the selected sound"));
        previewLoopButton_->setCheckable(true);
        previewLoopButton_->setChecked(false);
        previewLoopButton_->setEnabled(false);
        // The normal checked treatment is intentionally subtle. A preview loop
        // can otherwise be left running accidentally, so make its latched state
        // unmistakable while retaining the application's standard button shape.
        previewLoopButton_->setStyleSheet(QStringLiteral(
            "QToolButton:checked {"
            " background-color: #315a78;"
            " border-color: #8bb8d8;"
            " color: #ffffff;"
            " }"));
        soundPreviewControlsLayout->addWidget(previewLoopButton_);

        auto* volumeLabel = new QLabel(QStringLiteral("Volume:"), soundPreviewControls);
        soundPreviewControlsLayout->addWidget(volumeLabel);

        QSettings settings = settingsStore();
        previewVolumePercent_ = std::clamp(
            settings.value(kSoundPreviewVolumeSettingsKey, 100).toInt(),
            0,
            100);
        previewVolumeSlider_ = new QSlider(Qt::Horizontal, soundPreviewControls);
        previewVolumeSlider_->setObjectName(QStringLiteral("resourcePickerVolumeSlider"));
        previewVolumeSlider_->setRange(0, 100);
        previewVolumeSlider_->setValue(previewVolumePercent_);
        previewVolumeSlider_->setFixedWidth(120);
        previewVolumeSlider_->setToolTip(QStringLiteral("Sound preview volume"));
        soundPreviewControlsLayout->addWidget(previewVolumeSlider_);

        previewVolumeValue_ = new QLabel(
            QStringLiteral("%1%").arg(previewVolumePercent_),
            soundPreviewControls);
        previewVolumeValue_->setObjectName(QStringLiteral("resourcePickerVolumeValue"));
        previewVolumeValue_->setMinimumWidth(
            QFontMetrics(previewVolumeValue_->font()).horizontalAdvance(QStringLiteral("100%")));
        previewVolumeValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        soundPreviewControlsLayout->addWidget(previewVolumeValue_);
        soundPreviewControls->setVisible(hasSoundPreviewControls);
        previewHeaderLayout->addWidget(soundPreviewControls);

        const bool needsThingPreview = spec_.showModelPreview || options_.kind == ResourcePickerKind::template_;
        // Projection/shading controls belong to the shared 3D preview (3DO,
        // template, and sprite), never to text/audio-only preview pages.
        const bool hasModelPreviewControls = needsThingPreview;
        auto* modelPreviewControls = new QWidget(previewHeader);
        modelPreviewControls->setObjectName(QStringLiteral("resourcePickerModelPreviewControls"));
        auto* modelPreviewControlsLayout = new QHBoxLayout(modelPreviewControls);
        modelPreviewControlsLayout->setContentsMargins(0, 0, 0, 0);
        modelPreviewControlsLayout->setSpacing(8);

        previewProjectionCombo_ = new QComboBox(modelPreviewControls);
        previewProjectionCombo_->setObjectName(QStringLiteral("resourcePickerPreviewProjection"));
        previewProjectionCombo_->addItem("Orthographic", static_cast<int>(app::ViewportCamera::ProjectionType::Orthographic));
        previewProjectionCombo_->addItem("Perspective", static_cast<int>(app::ViewportCamera::ProjectionType::Perspective));
        previewProjectionCombo_->setToolTip("Preview projection");
        modelPreviewControlsLayout->addWidget(previewProjectionCombo_);

        previewFitButton_ = new QToolButton(modelPreviewControls);
        previewFitButton_->setObjectName(QStringLiteral("resourcePickerPreviewFit"));
        previewFitButton_->setText("Fit");
        previewFitButton_->setToolTip("Fit preview");
        modelPreviewControlsLayout->addWidget(previewFitButton_);

        previewRenderCombo_ = new QComboBox(modelPreviewControls);
        previewRenderCombo_->setObjectName(QStringLiteral("resourcePickerPreviewRenderMode"));
        previewRenderCombo_->addItem("Textured", 1);
        previewRenderCombo_->addItem("Solid", 0);
        previewRenderCombo_->addItem("Wireframe", -1);
        previewRenderCombo_->addItem("Vertices", -2);
        previewRenderCombo_->setToolTip("Preview shading");
        modelPreviewControlsLayout->addWidget(previewRenderCombo_);
        modelPreviewControls->setVisible(hasModelPreviewControls);
        previewHeaderLayout->addWidget(modelPreviewControls);
        rightLayout->addWidget(previewHeader);

        storageMetadataLabel_ = new QLabel(rightPane);
        storageMetadataLabel_->setObjectName(QStringLiteral("resourcePickerStorageMetadata"));
        storageMetadataLabel_->setTextFormat(Qt::PlainText);
        storageMetadataLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        storageMetadataLabel_->setWordWrap(true);
        storageMetadataLabel_->setMinimumWidth(0);
        storageMetadataLabel_->setMaximumWidth(480);
        storageMetadataLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        storageMetadataLabel_->setStyleSheet(QStringLiteral("color: #c8cbd0;"));
        storageMetadataLabel_->hide();

        previewStack_ = new QStackedWidget(rightPane);
        previewStack_->setMinimumHeight(240);
        previewStack_->setMaximumHeight(280);

        matPreviewLabel_ = new QLabel(previewStack_);
        matPreviewLabel_->setAlignment(Qt::AlignCenter);
        matPreviewLabel_->setStyleSheet("QLabel { background: #202225; border: 1px solid rgba(255,255,255,40); }");
        previewStack_->addWidget(matPreviewLabel_);

        if (needsThingPreview)
        {
            thingPreview_ = new ThingPreviewWidget(resources_, options_, previewStack_);
            thingPreview_->setResourceArchiveReloadCallback(
                [this]()
                {
                    return reloadArchiveResources();
                });
            thingPreview_->setResourceArchiveChangedCallback(
                [this]()
                {
                    handleResourceDiskChanged();
                });
            if (options_.kind == ResourcePickerKind::model3do
                || options_.kind == ResourcePickerKind::template_)
            {
                thingPreview_->prepareInteractiveGlHost();
            }
            previewStack_->addWidget(thingPreview_);
        }

        textPreview_ = new QPlainTextEdit(previewStack_);
        textPreview_->setReadOnly(true);
        textPreview_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        previewStack_->addWidget(textPreview_);
        onUiNumericFormatChanged(textPreview_, [this]() { refreshSoundPreviewText(); });

        rightLayout->addWidget(previewStack_);
        rightLayout->addWidget(storageMetadataLabel_);

        resourceStack_ = new QStackedWidget(rightPane);
        QPalette loadingPalette = resourceStack_->palette();
        loadingPalette.setColor(QPalette::Window, QColor(32, 32, 32));
        loadingPalette.setColor(QPalette::WindowText, QColor(164, 168, 174));
        resourceStack_->setPalette(loadingPalette);
        resourceStack_->setAutoFillBackground(true);

        resourceLoadingLabel_ = new QLabel(
            QStringLiteral("Loading %1 resources...").arg(spec_.displayName.toLower()),
            resourceStack_);
        resourceLoadingLabel_->setAlignment(Qt::AlignCenter);
        resourceLoadingLabel_->setPalette(loadingPalette);
        resourceLoadingLabel_->setAutoFillBackground(true);
        resourceLoadingLabel_->setStyleSheet(QStringLiteral(
            "QLabel { background-color: #202020; color: #a4a8ae; border: 1px solid #414141; }"));
        resourceStack_->addWidget(resourceLoadingLabel_);

        resourcesList_ = new detail::CategoryAssignableListWidget(resourceStack_);
        resourcesList_->setEnabled(false);
        static_cast<detail::CategoryAssignableListWidget*>(resourcesList_)->setCategoryMultiSelectMode(false);
        resourcesList_->setItemDelegate(new PickerTileSelectionDelegate(resourcesList_));
        resourcesList_->setMovement(QListView::Static);
        resourcesList_->setResizeMode(QListView::Adjust);
        resourcesList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        resourcesList_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        resourcesList_->setWordWrap(false);
        resourcesList_->setSpacing(2);
        resourcesList_->setContextMenuPolicy(Qt::CustomContextMenu);
        resourcesList_->viewport()->installEventFilter(this);
        static_cast<detail::CategoryAssignableListWidget*>(resourcesList_)->setCategoryDragNameProvider(
            [this](const QListWidgetItem* item)
            {
                if (!item)
                {
                    return QString{};
                }
                const QVariant actual = item->data(kActualNameRole);
                return actual.isValid() ? actual.toString().trimmed() : item->text().trimmed();
            });
        resourceStack_->addWidget(resourcesList_);
        resourceStack_->setCurrentWidget(resourceLoadingLabel_);
        rightLayout->addWidget(resourceStack_, 1);

        const auto noteThumbnailListInteraction = [this](const int)
        {
            templatePreviewPublicationPacer_.noteInteraction();
            if (options_.kind == ResourcePickerKind::template_
                || options_.kind == ResourcePickerKind::model3do)
            {
                scheduleTemplatePreviewLoad(
                    templatePreviewPublicationPacer_.delayUntilQuiet());
            }
        };
        connect(resourcesList_->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            noteThumbnailListInteraction);
        connect(resourcesList_->horizontalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            noteThumbnailListInteraction);

        thumbLoadTimer_.setSingleShot(true);
        connect(&thumbLoadTimer_, &QTimer::timeout, this, [this]()
            {
                processMatThumbLoadBatch();
            });
        templatePreviewLoadTimer_.setSingleShot(true);
        connect(&templatePreviewLoadTimer_, &QTimer::timeout, this, [this]()
            {
                processTemplatePreviewLoadBatch();
            });
        selectedModelPreviewRetryTimer_.setSingleShot(true);
        selectedModelPreviewRetryTimer_.setInterval(30);
        connect(&selectedModelPreviewRetryTimer_, &QTimer::timeout,
            this, [this]()
            {
                updatePreview();
            });
        previewResourceDiskWatcher_ = new detail::PreviewResourceDiskWatcher(
            this,
            [this]()
            {
                handleResourceDiskChanged();
            });
        previewResourceDiskWatcher_->setObjectName(
            QStringLiteral("resourcePickerPreviewDiskWatcher"));
        previewResourceDiskWatcher_->setArchiveChangeCallback(
            [this]()
            {
                return reloadArchiveResources();
            });
        previewResourceDiskWatcher_->setResourceLocator(resources_.get());
        if (options_.kind == ResourcePickerKind::model3do
            || options_.kind == ResourcePickerKind::template_
            || options_.kind == ResourcePickerKind::material
            || options_.kind == ResourcePickerKind::spr)
        {
            previewResourceDiskWatcher_->watchResource(options_.previewCmp, ".cmp");
        }
        templatePreviewKeepaliveTimer_.setInterval(150);
        connect(&templatePreviewKeepaliveTimer_, &QTimer::timeout, this, [this]()
            {
                if (closing_
                    || (filterRefreshTimer_ && filterRefreshTimer_->isActive()))
                {
                    return;
                }
                const bool repaired = requeueVisibleLoadingTemplatePreviewRows();
                if ((repaired || templatePreviewQueue_.pendingCount() > 0)
                    && templatePreviewActiveJobs_ <= 0
                    && !templatePreviewLoadTimer_.isActive())
                {
                    scheduleTemplatePreviewLoad(0);
                }
            });
        templatePreviewKeepaliveTimer_.start();
        if (QScrollBar* scroll = resourcesList_->verticalScrollBar())
        {
            scroll->setSingleStep(32);
            connect(scroll, &QScrollBar::valueChanged, this, [this](int)
                {
                    kickVisibleThumbLoads(0);
                });
        }
        if (QScrollBar* scroll = resourcesList_->horizontalScrollBar())
        {
            scroll->setSingleStep(32);
            connect(scroll, &QScrollBar::valueChanged, this, [this](int)
                {
                    kickVisibleThumbLoads(0);
                });
        }

        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);

        auto* bottom = new QWidget(this);
        auto* bottomLayout = new QHBoxLayout(bottom);
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(6);

        bottomLayout->addWidget(new QLabel("Mask:", bottom));

        filterEdit_ = new QLineEdit(bottom);
        filterEdit_->setObjectName(QStringLiteral("resourcePickerFilter"));
        filterEdit_->setPlaceholderText("Search...");
        filterEdit_->setFixedWidth(210);
        bottomLayout->addWidget(filterEdit_);

        filterRefreshTimer_ = new QTimer(this);
        filterRefreshTimer_->setObjectName(
            QStringLiteral("resourcePickerFilterRefreshTimer"));
        filterRefreshTimer_->setSingleShot(true);
        filterRefreshTimer_->setInterval(120);
        connect(filterRefreshTimer_, &QTimer::timeout, this, [this]()
            {
                constexpr int kSearchThumbnailResumeDelayMs = 260;
                rebuildResourceList(false, kSearchThumbnailResumeDelayMs);
            });

        auto* sortLabel = new QLabel(QStringLiteral("Sort:"), bottom);
        bottomLayout->addWidget(sortLabel);
        sortCombo_ = new QComboBox(bottom);
        sortCombo_->setObjectName(QStringLiteral("resourcePickerSort"));
        sortCombo_->addItem(QStringLiteral("Name (A-Z)"),
            static_cast<int>(PickerSortMode::nameAscending));
        sortCombo_->addItem(QStringLiteral("Name (Z-A)"),
            static_cast<int>(PickerSortMode::nameDescending));
        sortCombo_->addItem(QStringLiteral("Source order"),
            static_cast<int>(PickerSortMode::sourceOrder));
        sortCombo_->setToolTip(QStringLiteral(
            "Sort alphabetically or preserve the order from the selected category/resource list."));
        bottomLayout->addWidget(sortCombo_);

        selectedEdit_ = new QLineEdit(bottom);
        selectedEdit_->setPlaceholderText(QString("%1").arg(spec_.displayName));
        bottomLayout->addWidget(selectedEdit_, 1);

        layoutButton_ = new QToolButton(bottom);
        layoutButton_->setText("Layout");
        layoutButton_->setPopupMode(QToolButton::InstantPopup);
        layoutMenu_ = new ui::shortcuts::KeySequenceDisplayMenu(layoutButton_);
        layoutButton_->setMenu(layoutMenu_);
        bottomLayout->addWidget(layoutButton_);

        buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, bottom);
        bottomLayout->addWidget(buttons_);

        root->addWidget(bottom);

        okButton_ = buttons_->button(QDialogButtonBox::Ok);
        if (okButton_)
        {
            okButton_->setEnabled(false);
        }

        setupLayoutMenu();
        applyMatLayoutVisibility();

        connect(filterEdit_, &QLineEdit::textChanged, this, [this]()
            {
                templatePreviewPublicationPacer_.noteInteraction();
                clearMatThumbPending();
                clearTemplatePreviewPending();
                filterRefreshTimer_->start();
            });
        connect(sortCombo_, &QComboBox::currentIndexChanged, this, [this](const int)
            {
                applyResourceSort();
                rebuildResourceList(false);
            });

        connect(previewProjectionCombo_, &QComboBox::currentIndexChanged, this, [this](const int)
            {
                if (!thingPreview_ || !previewProjectionCombo_)
                {
                    return;
                }

                const auto type = static_cast<app::ViewportCamera::ProjectionType>(
                    previewProjectionCombo_->currentData().toInt());
                thingPreview_->setPreviewProjectionType(type);
                updatePreviewControls();
            });

        connect(previewRenderCombo_, &QComboBox::currentIndexChanged, this, [this](const int)
            {
                if (!thingPreview_ || !previewRenderCombo_)
                {
                    return;
                }

                thingPreview_->setPreviewRenderMode(previewRenderCombo_->currentData().toInt());
                updatePreviewControls();
            });

        connect(previewFitButton_, &QToolButton::clicked, this, [this]()
            {
                if (thingPreview_)
                {
                    thingPreview_->fitPreview();
                }
                updatePreviewControls();
            });

        connect(previewVolumeSlider_, &QSlider::valueChanged, this, [this](const int value)
            {
                previewVolumePercent_ = std::clamp(value, 0, 100);
                if (previewVolumeValue_)
                {
                    previewVolumeValue_->setText(QStringLiteral("%1%").arg(previewVolumePercent_));
                }

                QSettings settings = settingsStore();
                settings.setValue(kSoundPreviewVolumeSettingsKey, previewVolumePercent_);
                audio::AppSoundPlayer::instance().setPlaybackVolume(
                    audio::SoundPlaybackOwner::resourcePicker,
                    soundPreviewVolume());
            });

        connect(previewReplayButton_, &QToolButton::clicked, this, [this]()
            {
                if (!soundPreviewPlayable_)
                {
                    return;
                }

                if ( soundPlayingSelection_ != soundPreviewSelection_ ||
                     !audio::AppSoundPlayer::instance().isOwnedBy(audio::SoundPlaybackOwner::resourcePicker) )
                {
                    updatePreview(true);
                }
                else if (!audio::AppSoundPlayer::instance().replay(
                        audio::SoundPlaybackOwner::resourcePicker,
                        soundPreviewVolume()))
                {
                    // Playback ownership can be released independently of the
                    // modal picker. Re-read the selection in that rare case.
                    updatePreview(true);
                }
            });

        connect(previewLoopButton_, &QToolButton::toggled, this, [this](const bool checked)
            {
                soundPreviewLoop_ = checked;
                if ( soundPreviewPlayable_ && soundPlayingSelection_ == soundPreviewSelection_ &&
                     audio::AppSoundPlayer::instance().isPlaying(audio::SoundPlaybackOwner::resourcePicker) )
                {
                    // Rebuild the voice immediately so enabling Loop starts at
                    // the beginning and disabling it returns to a finite preview.
                    updatePreview(true);
                }
            });

        connect(selectedEdit_, &QLineEdit::textChanged, this, [this](const QString&)
            {
                updateOkState();
            });

        connect(selectedEdit_, &QLineEdit::returnPressed, this, [this]()
            {
                const QString desired = normalizeChosenName();
                if (desired.isEmpty())
                {
                    return;
                }
                selectResource(desired);
            });

        connect(sources_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*)
            {
                refreshResourcesForSource();
            });
        connect(sources_->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this](QWidget*, QAbstractItemDelegate::EndEditHint)
            {
                finalizePendingCategoryEdit();
            });

        connect(sources_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
            {
                showCategoryContextMenu(pos);
            });
        static_cast<detail::CategoryTreeWidget*>(sources_)->setCategoryDropHandler(
            [this](QTreeWidgetItem* item, const QStringList& names, const Qt::DropAction action)
            {
                return handleCategoryDrop(item, names, action);
            });
        static_cast<detail::CategoryTreeWidget*>(sources_)->setCategoryDropActionResolver(
            [this](QTreeWidgetItem* item, const QStringList& names, const Qt::KeyboardModifiers modifiers)
            {
                return resolveCategoryDropAction(item, names, modifiers);
            });
        connect(resourcesList_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
            {
                showResourceMembershipContextMenu(pos);
            });

        connect(resourcesList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem*, QListWidgetItem*)
            {
                const QString current = currentResourceName();
                if (selectedEdit_)
                {
                    QSignalBlocker blocker(selectedEdit_);
                    selectedEdit_->setText(current);
                }
                updateOkState();
                updateStorageMetadata(current);
                updatePreview(true);
                kickVisibleThumbLoads(0);
            });

        connect(resourcesList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*)
            {
                accept();
            });

        connect(buttons_, &QDialogButtonBox::accepted, this, [this]()
            {
                accept();
            });
        connect(buttons_, &QDialogButtonBox::rejected, this, [this]()
            {
                reject();
            });

        updatePreviewControls();
        setProperty(
            "resourcePickerResourceSnapshotAddressForTest",
            QVariant::fromValue<quintptr>(
                reinterpret_cast<quintptr>(resources_.get())));
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    QImage materialImageForTest(const QString& name)
    { return loadMatImage(name).value_or(QImage{}); }
    std::vector<QImage> spriteAnimationPaletteImagesForTest(const QString& name)
    {
        watchSpritePreviewDependencies(name.toStdString());
        thingPreview_->setPreviewResource(ResourcePickerKind::spr, name, {});
        return thingPreview_->animationFramesForTest();
    }
    detail::PreviewResourceDiskWatcher* dependencyWatcherForTest()
    { return previewResourceDiskWatcher_; }

    std::optional<std::string> resourceTextForTest(const std::string& name) const
    {
        return resources_->tryReadText(name);
    }

    std::vector<std::string> visibleResourceNamesForTest()
    {
        initializeResourceData();
        std::vector<std::string> names{};
        if ( !resourcesList_ )
        {
            return names;
        }
        names.reserve(static_cast<std::size_t>(resourcesList_->count()));
        for ( int row = 0; row < resourcesList_->count(); ++row )
        {
            const QListWidgetItem* item = resourcesList_->item(row);
            if ( !item )
            {
                continue;
            }
            const QString actualName = item->data(kActualNameRole).toString().trimmed();
            names.push_back((actualName.isEmpty() ? item->text() : actualName).toStdString());
        }
        return names;
    }

    ResourcePickerStorageMetadataTestSnapshot storageMetadataStepForTest(
        const ResourcePickerStorageMetadataTestStep& step)
    {
        initializeResourceData();
        ResourcePickerStorageMetadataTestSnapshot result{};
        if ( !sources_ || !resourcesList_ )
        {
            return result;
        }

        QTreeWidgetItem* source = nullptr;
        for ( int index = 0; index < sources_->topLevelItemCount(); ++index )
        {
            QTreeWidgetItem* const candidate = sources_->topLevelItem(index);
            if ( candidate
                && candidate->data(0, kSourceKindRole).toInt() == step.sourceKind
                && candidate->data(0, kSourceIndexRole).toInt() == step.sourceIndex )
            {
                source = candidate;
                break;
            }
        }
        if ( !source )
        {
            return result;
        }
        result.sourceFound = true;
        sources_->setCurrentItem(source);

        if ( step.beforeSelect )
        {
            step.beforeSelect();
        }

        QListWidgetItem* selected = nullptr;
        if ( !step.resourceName.empty() )
        {
            const QString desired = QString::fromStdString(step.resourceName);
            for ( int index = 0; index < resourcesList_->count(); ++index )
            {
                QListWidgetItem* const candidate = resourcesList_->item(index);
                if ( !candidate )
                {
                    continue;
                }
                const QString actual = candidate->data(kActualNameRole).toString().trimmed();
                if ( (!actual.isEmpty() && actual.compare(desired, Qt::CaseInsensitive) == 0)
                    || candidate->text().compare(desired, Qt::CaseInsensitive) == 0 )
                {
                    selected = candidate;
                    break;
                }
            }
        }

        result.itemFound = selected != nullptr || step.resourceName.empty();
        {
            const QSignalBlocker listSignals(resourcesList_);
            resourcesList_->setCurrentItem(selected);
        }
        if ( selectedEdit_ )
        {
            const QSignalBlocker editSignals(selectedEdit_);
            selectedEdit_->setText(currentResourceName());
        }
        result.itemBold = selected && selected->font().bold();
        updateOkState();
        updateStorageMetadata(currentResourceName());
        // This drives the same selection preview update as the real picker,
        // while keeping WAV selection silent in the test harness. One focused
        // case can update only the storage label to prove provenance lookup
        // does not materialize a lazy primary-resource payload.
        if ( step.runPreviewUpdate )
        {
            updatePreview(false);
        }

        const QLabel* const metadata = findChild<QLabel*>(
            QStringLiteral("resourcePickerStorageMetadata"));
        result.labelFound = metadata != nullptr;
        if ( metadata )
        {
            result.text = metadata->text().toStdString();
            result.labelWordWrap = metadata->wordWrap();
            result.labelMaximumWidth = metadata->maximumWidth();
        }
        return result;
    }

    QImage thingPreviewFrameForTest()
    {
        initializeResourceData();
        return thingPreview_ ? thingPreview_->previewFrameForTest() : QImage{};
    }

    bool thingPreviewInteractiveForTest() const noexcept
    {
        return thingPreview_ && thingPreview_->hasInteractiveGlPreview();
    }

    bool thingPreviewTexturesForTest() const noexcept
    {
        return thingPreview_ && thingPreview_->texturesEnabledForTest();
    }

    bool thingPreviewSolidForTest() const noexcept
    {
        return thingPreview_ && thingPreview_->solidEnabledForTest();
    }

    int thingPreviewRenderModeForTest() const noexcept
    {
        return thingPreview_ ? thingPreview_->renderModeForTest() : -99;
    }

    std::string thingPreviewSelectedNameForTest() const
    {
        return currentResourceName().toStdString();
    }

    void freezeThingPreviewAutoRotationForTest()
    {
        if ( thingPreview_ )
        {
            thingPreview_->freezeAutoRotationForTest();
        }
    }

    detail::PreviewResourceDiskWatcher* thingPreviewWatcherForTest() const noexcept
    {
        return thingPreview_ ? thingPreview_->resourceDiskWatcherForTest() : nullptr;
    }

    int archiveReloadAttemptCountForTest() const noexcept
    {
        return archiveReloadAttemptCountForTest_;
    }

    int templatePreviewActiveJobsForTest() const noexcept
    {
        return templatePreviewActiveJobs_;
    }

    bool archiveReloadWarningShownForTest() const noexcept
    {
        return archiveReloadWarningShown_;
    }

    std::shared_ptr<const assets::ResourceLocator> previewResourcesForTest(
        const int sourceKind, const int sourceIndex)
    {
        initializeResourceData();
        for (int i = 0; i < sources_->topLevelItemCount(); ++i)
        {
            auto* item = sources_->topLevelItem(i);
            if (item->data(0, kSourceKindRole).toInt() == sourceKind
                && item->data(0, kSourceIndexRole).toInt() == sourceIndex)
            {
                sources_->setCurrentItem(item);
                const QString selected = currentResourceName().isEmpty() && !resourcesAll_.empty()
                    ? resourcesAll_.front() : currentResourceName();
                return previewResourcesForResource(selected.toStdString());
            }
        }
        return {};
    }
#endif

    ~ResourcePickerDialog() override
    {
        if ( thingPreview_ )
        {
            thingPreview_->setResourceArchiveReloadCallback({});
            thingPreview_->setResourceArchiveChangedCallback({});
        }
        if ( previewResourceDiskWatcher_ )
        {
            previewResourceDiskWatcher_->setArchiveChangeCallback({});
        }
        stopDeferredPreviewWork();
        stopSoundPreview();
    }

    std::optional<std::string> result() const
    {
        if (pickedName_.empty())
        {
            return std::nullopt;
        }
        return pickedName_;
    }

protected:
    void done(const int result) override
    {
        closing_ = true;
        stopDeferredPreviewWork();
        QDialog::done(result);
    }

    void showEvent(QShowEvent* event) override
    {
        QDialog::showEvent(event);
        if (initialPopulationComplete_ || initialPopulationScheduled_)
        {
            return;
        }

        // Leave the event loop free to paint the opaque loading page before
        // resource discovery and thumbnail scheduling begin.
        initialPopulationScheduled_ = true;
        QTimer::singleShot(kInitialPopulationDelayMs, this, [this]()
            {
                initializeResourceData();
            });
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (resourcesList_ && watched == resourcesList_->viewport() && event)
        {
            switch (event->type())
            {
                case QEvent::Wheel:
                {
                    templatePreviewPublicationPacer_.noteInteraction();
                    auto* wheelEvent = static_cast<QWheelEvent*>(event);
                    if ((wheelEvent->modifiers() & Qt::ShiftModifier) != 0)
                    {
                        if (QScrollBar* scroll = resourcesList_->verticalScrollBar())
                        {
                            const QPoint delta = wheelEvent->angleDelta();
                            const int dominantDelta = (std::abs(delta.y()) >= std::abs(delta.x()))
                                ? delta.y()
                                : delta.x();
                            if (dominantDelta != 0)
                            {
                                const int direction = dominantDelta < 0 ? 1 : -1;
                                const int pageStep = detail::previewThumbnailPageStep(
                                    scroll->pageStep(),
                                    resourcesList_->gridSize().height());
                                scroll->setValue(scroll->value() + direction * pageStep);
                            }
                        }
                        kickVisibleThumbLoads(0);
                        wheelEvent->accept();
                        return true;
                    }
                    kickVisibleThumbLoads(0);
                    break;
                }
                case QEvent::Resize:
                case QEvent::Show:
                case QEvent::LayoutRequest:
                    updateListModeGridForViewport();
                    kickVisibleThumbLoads(0);
                    break;
                default:
                    break;
            }
        }
        return QDialog::eventFilter(watched, event);
    }

    void accept() override
    {
        const QString selectedName = normalizeChosenName();
        if (selectedName.isEmpty())
        {
            return;
        }

        if (options_.kind == ResourcePickerKind::template_
            && !options_.allowBaseTemplates
            && isBaseTemplateName(selectedName))
        {
            QMessageBox::information(
                this,
                "Pick template",
                QString("Base templates cannot be picked here:\n%1").arg(selectedName));
            return;
        }

        if (!resourceExists(selectedName))
        {
            QMessageBox::information(
                this,
                "Pick resource",
                QString("Resource not found in currently mounted resources:\n%1").arg(selectedName));
            return;
        }

        pickedName_ = selectedName.toStdString();
        stopSoundPreview();
        QDialog::accept();
    }

    void reject() override
    {
        stopSoundPreview();
        QDialog::reject();
    }

private:
    void handleResourceDiskChanged()
    {
        if ( options_.kind == ResourcePickerKind::model3do
            || options_.kind == ResourcePickerKind::template_ )
        {
            handleModel3doDiskChanged();
            return;
        }

        // Non-thumbnail pickers still enumerate mounted archive entries.
        // Refresh the active list and lightweight previews when the
        // dialog-owned locator is remounted.
        selectedSourcePreviewResources_.clear();
        sourceResourcesCache_.clear();
        diskResourceNames_.clear();
        diskResourceNamesResolved_ = false;
        matThumbCache_.clear();
        spriteThumbCache_.clear();
        previewPalette_.reset();
        previewPaletteResolved_ = false;
        if ( thingPreview_ )
        {
            thingPreview_->invalidateResourceCaches();
        }
        if ( options_.kind == ResourcePickerKind::spr )
        {
            detail::clearSharedPreviewAssetCaches();
        }
        refreshResourcesForSource();
        updatePreview();
    }

    bool reloadArchiveResources()
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        ++archiveReloadAttemptCountForTest_;
#endif
        if ( templatePreviewActiveJobs_ > 0 )
        {
            return false;
        }
        try
        {
            // The modal picker owns a locator snapshot so its GOB source rows
            // and thumbnail workers do not depend on a separate MainWindow
            // watcher being alive.
            auto reloaded =
                std::make_shared<sed::assets::ResourceLocator>(*resources_);
            reloaded->reloadGobs();
            modelThumbnailRenderer_.reset();
            resources_ = std::move(reloaded);
            thumbnailResources_ = resources_;
            selectedSourcePreviewResources_.clear();
            previewResourceDiskWatcher_->setResourceLocator(resources_.get());
            if ( thingPreview_ )
            {
                thingPreview_->setResourceSnapshot(resources_);
            }
            archiveReloadWarningShown_ = false;
            return true;
        }
        catch ( const std::exception& e )
        {
            if ( !archiveReloadWarningShown_ )
            {
                qCWarning(logResources).noquote()
                    << "Mounted resource archive could not be reloaded yet:"
                    << QString::fromUtf8(e.what());
                archiveReloadWarningShown_ = true;
            }
            // Keep the watcher event pending. ResourceLocator has
            // transactional GOB replacement, so the old index remains
            // authoritative until a later retry succeeds.
            return false;
        }
    }

    PickerSortMode resourceSortMode() const noexcept
    {
        return sortCombo_
            ? static_cast<PickerSortMode>(sortCombo_->currentData().toInt())
            : PickerSortMode::nameAscending;
    }

    void applyResourceSort()
    {
        resourcesAll_ = resourcesSourceOrder_;
        const PickerSortMode mode = resourceSortMode();
        if (mode == PickerSortMode::sourceOrder)
        {
            return;
        }
        std::stable_sort(resourcesAll_.begin(), resourcesAll_.end(),
            [mode](const QString& lhs, const QString& rhs)
            {
                const int compared = lhs.compare(rhs, Qt::CaseInsensitive);
                return mode == PickerSortMode::nameAscending
                    ? compared < 0
                    : compared > 0;
            });
    }

    void commitResourceSourceOrder()
    {
        resourcesSourceOrder_ = resourcesAll_;
        applyResourceSort();
        rebuildResourceList();
    }

    double soundPreviewVolume() const noexcept
    {
        return static_cast<double>(previewVolumePercent_) / 100.0;
    }

    void stopSoundPreview() noexcept
    {
        if (options_.kind == ResourcePickerKind::wav)
        {
            audio::AppSoundPlayer::instance().stop(audio::SoundPlaybackOwner::resourcePicker);
            soundPreviewPlayable_ = false;
            soundPlayingSelection_.clear();
            if (previewReplayButton_)
            {
                previewReplayButton_->setEnabled(false);
            }
        }
    }

    void initializeResourceData()
    {
        if (initialPopulationComplete_)
        {
            return;
        }

        // Build the source tree without triggering its current-item callback;
        // the initial resource list is populated exactly once below.
        {
            QSignalBlocker sourceSignals(sources_);
            rebuildSources();
        }
        refreshResourcesForSource();

        const QString initial = fileNameOnly(QString::fromStdString(options_.initialName));
        const bool initialIsTemplateNone =
            options_.kind == ResourcePickerKind::template_
            && initial.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0;
        if (!initial.isEmpty() && !initialIsTemplateNone)
        {
            selectedEdit_->setText(initial);
            // Preserve the initial row while population signals are blocked.
            // This initializer already runs after the dark loading surface's
            // delayed first paint; preview preparation is queued below.
            const QSignalBlocker listSignals(resourcesList_);
            selectResource(initial);
        }

        initialPopulationComplete_ = true;
        resourcesList_->setEnabled(true);
        if (resourceStack_)
        {
            resourceStack_->setCurrentWidget(resourcesList_);
        }
        if (!initial.isEmpty() && !initialIsTemplateNone)
        {
            updateOkState();
            updateStorageMetadata(currentResourceName());
            // The loading surface has already had its delayed first paint.
            // Route the selected resource into preview preparation before
            // background thumbnail batches begin. A cold model only queues
            // its bounded parse job here; a warm model can display at once.
            updatePreview();
            updatePreviewControls();
        }
        kickVisibleThumbLoads(0);
    }

    QString currentResourceName() const
    {
        if (!resourcesList_)
        {
            return {};
        }
        const QListWidgetItem* item = resourcesList_->currentItem();
        if (!item)
        {
            return {};
        }
        const QVariant actual = item->data(kActualNameRole);
        return actual.isValid() ? actual.toString().trimmed() : item->text().trimmed();
    }

    SourceKind currentSourceKind() const
    {
        if (!sources_ || !sources_->currentItem())
        {
            return SourceKind::all;
        }
        return static_cast<SourceKind>(sources_->currentItem()->data(0, kSourceKindRole).toInt());
    }

    int currentSourceIndex() const
    {
        if (!sources_ || !sources_->currentItem())
        {
            return -1;
        }
        return sources_->currentItem()->data(0, kSourceIndexRole).toInt();
    }

    std::shared_ptr<const assets::ResourceLocator> previewResourcesForResource(
        const std::string_view name)
    {
        if ((options_.kind != ResourcePickerKind::model3do && options_.kind != ResourcePickerKind::spr)
            || (currentSourceKind() == SourceKind::all && !options_.preferredDiskRoot)
            || name.empty()) return thumbnailResources_;
        const auto sourceKind = currentSourceKind();
        const int sourceIndex = currentSourceIndex();
        const std::string key = resourceSourceCacheKey(sourceKind, sourceIndex)
            + "|" + sed::text::upperAscii(name)
            + "|base=" + std::to_string(resources_->previewCacheIdentity())
            + "|epoch=" + std::to_string(detail::currentSharedPreviewCacheEpoch());
        if (const auto found = selectedSourcePreviewResources_.find(key);
            found != selectedSourcePreviewResources_.end()) return found->second;

        auto snapshot = std::make_shared<assets::ResourceLocator>(*resources_);
        snapshot->setPrimaryResourceOverride(name,
            [base = resources_, spec = spec_, sourceKind, sourceIndex,
                preferred = options_.preferredDiskRoot, selected = std::string(name)]()
            {
                return tryReadBinaryFromSource(*base, spec, sourceKind, sourceIndex, preferred, selected);
            });
        // Register selected loose paths too: the normal lookup winner can be
        // another source, while directory watches alone miss content writes.
        if (previewResourceDiskWatcher_ && sourceKind == SourceKind::diskRoot)
        {
            const auto roots = collectDiskRoots(*resources_, options_.preferredDiskRoot, spec_.includeConfiguredDiskRoots);
            if (sourceIndex >= 0 && static_cast<std::size_t>(sourceIndex) < roots.size())
                for (const auto& path : candidateDiskPaths(roots[static_cast<std::size_t>(sourceIndex)], spec_, name))
                    previewResourceDiskWatcher_->watchResource(path.string());
        }
        if (selectedSourcePreviewResources_.size() >= 64u)
            selectedSourcePreviewResources_.erase(selectedSourcePreviewResources_.begin());
        selectedSourcePreviewResources_.emplace(key, snapshot);
        return snapshot;
    }

    QString ensureDefaultExtension(const QString& name) const
    {
        const QString base = fileNameOnly(name);
        if (base.isEmpty())
        {
            return {};
        }
        if (options_.kind == ResourcePickerKind::template_)
        {
            return base;
        }
        if (hasAnyExtension(base, spec_.extensions))
        {
            return base;
        }
        if (spec_.extensions.empty())
        {
            return base;
        }
        return base + QString::fromStdString(spec_.extensions.front());
    }

    QString normalizeChosenName() const
    {
        QString selected = currentResourceName();
        if (selected.isEmpty() && selectedEdit_)
        {
            selected = selectedEdit_->text().trimmed();
        }
        return ensureDefaultExtension(selected);
    }

    bool resourceExists(const QString& name) const
    {
        if (options_.kind == ResourcePickerKind::template_)
        {
            if (!options_.allowBaseTemplates && isBaseTemplateName(name))
            {
                return false;
            }
            return templateDatabase_.find(name.toStdString()) != nullptr;
        }
        const std::string key = name.toStdString();
        const auto bytes =
            tryReadBinaryFromSource(*resources_, spec_, SourceKind::all, -1, options_.preferredDiskRoot, key);
        return bytes.has_value();
    }

    void setupLayoutMenu()
    {
        if (!layoutButton_ || !layoutMenu_)
        {
            return;
        }

        auto makeLayoutAction = [&](const QString& text, const MatThumbLayout layout)
        {
            auto* action = layoutMenu_->addAction(text);
            action->setCheckable(true);
            connect(action, &QAction::triggered, this, [this, layout]()
                {
                    setThumbLayout(layout);
                });
            layoutActions_[static_cast<std::size_t>(layout)] = action;
        };

        makeLayoutAction("No thumbs", MatThumbLayout::none);
        makeLayoutAction("Small thumbs", MatThumbLayout::small);
        makeLayoutAction("Middle thumbs", MatThumbLayout::middle);
        makeLayoutAction("Large thumbs", MatThumbLayout::large);

        QSettings settings = settingsStore();
        const int stored = settings.value("resource_picker/mat_thumbnail_layout", static_cast<int>(MatThumbLayout::large)).toInt();
        const int clamped = std::clamp(stored, static_cast<int>(MatThumbLayout::none), static_cast<int>(MatThumbLayout::large));
        setThumbLayout(static_cast<MatThumbLayout>(clamped), false);
    }

    void setThumbLayout(const MatThumbLayout layout, const bool persist = true)
    {
        thumbLayout_ = layout;
        for (std::size_t i = 0; i < layoutActions_.size(); ++i)
        {
            QAction* action = layoutActions_[i];
            if (!action)
            {
                continue;
            }
            QSignalBlocker blocker(action);
            action->setChecked(i == static_cast<std::size_t>(thumbLayout_));
        }

        if (persist)
        {
            QSettings settings = settingsStore();
            settings.setValue("resource_picker/mat_thumbnail_layout", static_cast<int>(thumbLayout_));
        }

        if (spec_.showMatThumbs)
        {
            rebuildResourceList(false);
        }
    }

    void applyMatLayoutVisibility()
    {
        const bool visible = spec_.showMatThumbs;
        if (layoutButton_)
        {
            layoutButton_->setVisible(visible);
        }
    }

    bool useMatThumbnails() const
    {
        return spec_.showMatThumbs && thumbLayout_ != MatThumbLayout::none;
    }

    int thumbIconSize() const
    {
        switch (thumbLayout_)
        {
            case MatThumbLayout::small: return 56;
            case MatThumbLayout::middle: return 84;
            case MatThumbLayout::large: return 132;
            case MatThumbLayout::none:
            default:
                return 0;
        }
    }

    bool isResourceOnDisk(const QString& name)
    {
        if (!diskResourceNamesResolved_)
        {
            diskResourceNames_ = collectDiskResourceNameKeys(*resources_, spec_);
            diskResourceNamesResolved_ = true;
        }
        return diskResourceNames_.contains(sed::text::upperAscii(name.toStdString()));
    }

    QPixmap cachedMatThumbPixmap(const QString& name, const int iconSize) const
    {
        if (iconSize <= 0)
        {
            return QPixmap{};
        }

        const std::string cacheKey =
            sed::text::upperAscii(name.toStdString())
            + "|r="
            + detail::previewResourceRevisionToken(resources_.get(), name.toStdString(), ".mat")
            + "|cmp="
            + sed::text::upperAscii(options_.previewCmp)
            + "|cr="
            + detail::previewResourceRevisionToken(resources_.get(), options_.previewCmp, ".cmp")
            + "|"
            + std::to_string(iconSize);
        if (const auto it = matThumbCache_.find(cacheKey); it != matThumbCache_.end() && !it->second.isNull())
        {
            return it->second;
        }
        return QPixmap{};
    }

    QPixmap cachedSpriteThumbPixmap(const QString& name, const int iconSize) const
    {
        if (iconSize <= 0)
        {
            return QPixmap{};
        }

        const std::string cacheKey =
            sed::text::upperAscii(name.toStdString())
            + "|deps="
            + detail::sharedPreviewSpriteDependencyRevisionToken(
                resources_.get(), options_.previewCmp, name.toStdString())
            + "|"
            + std::to_string(iconSize);
        if (const auto it = spriteThumbCache_.find(cacheKey); it != spriteThumbCache_.end() && !it->second.isNull())
        {
            return it->second;
        }
        return QPixmap{};
    }

    QIcon matPlaceholderIcon(const int iconSize)
    {
        const int px = std::max(24, iconSize);
        if (!matPlaceholderIcon_.isNull() && matPlaceholderIconSize_ == px)
        {
            return matPlaceholderIcon_;
        }

        matPlaceholderIcon_ = QIcon(detail::makePreviewLoadingPixmap(QSize(px, px)));
        matPlaceholderIconSize_ = px;
        return matPlaceholderIcon_;
    }

    QIcon matUnavailableIcon(const int iconSize)
    {
        const int px = std::max(24, iconSize);
        if (!matUnavailableIcon_.isNull() && matUnavailableIconSize_ == px)
        {
            return matUnavailableIcon_;
        }

        matUnavailableIcon_ = QIcon(detail::makePreviewUnavailablePixmap(QSize(px, px)));
        matUnavailableIconSize_ = px;
        return matUnavailableIcon_;
    }

    void clearMatThumbPending()
    {
        thumbLoadTimer_.stop();
        thumbPendingFlags_.clear();
        thumbPendingCount_ = 0;
        thumbScanCursor_ = 0;
    }

    void scheduleMatThumbLoad(const int delayMs)
    {
        if (!useMatThumbnails() || thumbPendingCount_ <= 0)
        {
            thumbLoadTimer_.stop();
            return;
        }
        if (!thumbLoadTimer_.isActive())
        {
            thumbLoadTimer_.start(std::max(0, delayMs));
        }
    }

    bool visibleMatRowRange(int& rowBegin, int& rowEnd) const
    {
        return detail::visiblePreviewListRowRange(resourcesList_, rowBegin, rowEnd, 4);
    }

    bool loadMatThumbForRow(const int row, const int iconSize)
    {
        if (!resourcesList_ || row < 0 || row >= resourcesList_->count() || row >= static_cast<int>(thumbPendingFlags_.size()))
        {
            return false;
        }
        if (thumbPendingFlags_[static_cast<std::size_t>(row)] == 0)
        {
            return false;
        }

        QListWidgetItem* item = resourcesList_->item(row);
        if (item)
        {
            const QString actualName = item->data(kActualNameRole).toString().trimmed().isEmpty()
                ? item->text()
                : item->data(kActualNameRole).toString().trimmed();

            QPixmap icon{};
            if (options_.kind == ResourcePickerKind::spr)
            {
                watchSpritePreviewDependencies(actualName.toStdString());
                const std::string cacheKey =
                    sed::text::upperAscii(actualName.toStdString())
                    + "|deps="
                    + detail::sharedPreviewSpriteDependencyRevisionToken(
                        previewResourcesForResource(actualName.toStdString()).get(), options_.previewCmp, actualName.toStdString())
                    + "|"
                    + std::to_string(iconSize);
                if (const std::optional<QImage> spriteImage =
                        detail::loadSharedPreviewSpriteImage(previewResourcesForResource(actualName.toStdString()).get(), options_.previewCmp, actualName.toStdString()))
                {
                    icon = makeTemplatePreviewPixmap(*spriteImage, QSize(iconSize, iconSize));
                    spriteThumbCache_[cacheKey] = icon;
                }
            }
            else
            {
                icon = matThumbPixmap(actualName, iconSize);
            }

            item->setIcon(icon.isNull() ? matUnavailableIcon(iconSize) : QIcon(icon));
        }

        thumbPendingFlags_[static_cast<std::size_t>(row)] = 0;
        if (thumbPendingCount_ > 0)
        {
            --thumbPendingCount_;
        }
        return true;
    }

    void processMatThumbLoadBatch()
    {
        if (!useMatThumbnails() || !resourcesList_ || thumbPendingCount_ <= 0)
        {
            thumbLoadTimer_.stop();
            return;
        }

        const int itemCount = resourcesList_->count();
        const int iconPx = thumbIconSize();
        if (itemCount <= 0 || iconPx <= 0)
        {
            clearMatThumbPending();
            return;
        }

        const int batchCount = iconPx >= 120 ? 2 : (iconPx >= 84 ? 4 : 8);
        int loaded = 0;

        int rowBegin = -1;
        int rowEnd = -1;
        if (visibleMatRowRange(rowBegin, rowEnd))
        {
            const int margin = iconPx >= 120 ? 5 : 9;
            rowBegin = std::max(0, rowBegin - margin);
            rowEnd = std::min(itemCount - 1, rowEnd + margin);
            for (int row = rowBegin; row <= rowEnd && loaded < batchCount; ++row)
            {
                if (loadMatThumbForRow(row, iconPx))
                {
                    ++loaded;
                }
            }
        }

        int scanned = 0;
        while (loaded < batchCount && scanned < itemCount && thumbPendingCount_ > 0)
        {
            if (thumbScanCursor_ >= itemCount)
            {
                thumbScanCursor_ = 0;
            }
            const int row = thumbScanCursor_++;
            if (loadMatThumbForRow(row, iconPx))
            {
                ++loaded;
            }
            ++scanned;
        }

        if (thumbPendingCount_ > 0)
        {
            scheduleMatThumbLoad(8);
        }
    }

    void clearTemplatePreviewPending()
    {
        templatePreviewLoadTimer_.stop();
        templatePreviewQueue_.clear();
    }

    void stopDeferredPreviewWork()
    {
        if (filterRefreshTimer_)
        {
            filterRefreshTimer_->stop();
        }
        thumbLoadTimer_.stop();
        templatePreviewLoadTimer_.stop();
        templatePreviewKeepaliveTimer_.stop();
        selectedModelPreviewRetryTimer_.stop();
        clearMatThumbPending();
        clearTemplatePreviewPending();
    }

    void scheduleTemplatePreviewLoad(const int delayMs)
    {
        if (closing_
            || (filterRefreshTimer_ && filterRefreshTimer_->isActive())
            || (options_.kind != ResourcePickerKind::template_ && options_.kind != ResourcePickerKind::model3do)
            || templatePreviewQueue_.pendingCount() <= 0)
        {
            templatePreviewLoadTimer_.stop();
            return;
        }
        const auto kThumbTuning = detail::sharedModelThumbLoadTuning();
        if (!templatePreviewLoadTimer_.isActive())
        {
            templatePreviewLoadTimer_.start(std::max(kThumbTuning.startDelayMs, delayMs));
        }
    }

    void kickVisibleThumbLoads(const int delayMs)
    {
        if (closing_
            || (filterRefreshTimer_ && filterRefreshTimer_->isActive()))
        {
            return;
        }
        scheduleMatThumbLoad(delayMs);
        const bool repaired = requeueVisibleLoadingTemplatePreviewRows();
        if (repaired || templatePreviewQueue_.pendingCount() > 0)
        {
            scheduleTemplatePreviewLoad(delayMs);
        }
    }

    bool loadTemplatePreviewForRow(
        const int row,
        bool* renderedAuthoritativeThumbnail = nullptr)
    {
        if (renderedAuthoritativeThumbnail)
        {
            *renderedAuthoritativeThumbnail = false;
        }
        if (!resourcesList_ || row < 0 || row >= resourcesList_->count() || !templatePreviewQueue_.hasRow(row))
        {
            return false;
        }
        if (!templatePreviewQueue_.isPending(row))
        {
            return false;
        }

        const std::uint64_t listGeneration = resourceListGeneration_;
        QListWidgetItem* item = resourcesList_->item(row);
        if (!item)
        {
            return false;
        }

        const QString resourceName = item->data(kActualNameRole).toString().trimmed();
        const TemplateListModelPreviewMode thumbMode = loadTemplateListModelPreviewMode();
        bool assignedIcon = false;
        QIcon assignedPreviewIcon{};

        auto finishPendingRow = [&](const bool clearIcon)
        {
            if (listGeneration != resourceListGeneration_
                || !resourcesList_ || row < 0
                || row >= resourcesList_->count())
            {
                return;
            }
            QListWidgetItem* currentItem = resourcesList_->item(row);
            if (!currentItem
                || currentItem->data(kActualNameRole).toString().trimmed()
                    .compare(resourceName, Qt::CaseInsensitive) != 0)
            {
                return;
            }
            if (clearIcon)
            {
                detail::publishPreviewThumbnailItem(
                    resourcesList_,
                    currentItem,
                    matUnavailableIcon(resourcesList_->iconSize().width()),
                    kPreviewThumbStateRole,
                    static_cast<int>(PreviewThumbState::unavailable));
            }
            templatePreviewQueue_.finish(row);
        };

        auto refreshCurrentItem = [&]() -> bool
        {
            if (listGeneration != resourceListGeneration_
                || !resourcesList_ || row < 0
                || row >= resourcesList_->count())
            {
                return false;
            }
            item = resourcesList_->item(row);
            return item
                && item->data(kActualNameRole).toString().trimmed()
                    .compare(resourceName, Qt::CaseInsensitive) == 0;
        };

        if (options_.kind == ResourcePickerKind::model3do)
        {
            const QSize iconSize = resourcesList_->iconSize();
            if (thumbMode != TemplateListModelPreviewMode::none)
            {
                watchModelPreviewDependencies(resourceName.toStdString());
                QIcon icon{};
                const auto result = detail::loadSharedModelPreviewIcon(
                    templatePreviewIconCache_,
                    modelThumbnailRenderer_,
                    previewResourcesForResource(resourceName.toStdString()),
                    options_.previewProjectType,
                    options_.previewCmp,
                    resourceName.toStdString(),
                    iconSize,
                    thumbMode == TemplateListModelPreviewMode::wireframe,
                    templatePreviewActiveJobs_,
                    [this](const std::string& queuedModelName)
                    {
                        queueTemplateModelPrep(queuedModelName);
                    },
                    icon);
                if (!refreshCurrentItem())
                {
                    return false;
                }
                watchModelPreviewDependencies(resourceName.toStdString());
                switch (result)
                {
                    case detail::SharedModelPreviewIconLoadResult::assigned:
                        assignedPreviewIcon = icon;
                        assignedIcon = true;
                        break;
                    case detail::SharedModelPreviewIconLoadResult::rendered:
                        assignedPreviewIcon = icon;
                        assignedIcon = true;
                        if (renderedAuthoritativeThumbnail)
                        {
                            *renderedAuthoritativeThumbnail = true;
                        }
                        break;
                    case detail::SharedModelPreviewIconLoadResult::pending:
                        return false;
                    case detail::SharedModelPreviewIconLoadResult::unavailable:
                        finishPendingRow(true);
                        return false;
                }
            }
        }
        else
        {
            const ThingTemplate* tpl = templateDatabase_.find(resourceName.toStdString());
            if (!tpl)
            {
                finishPendingRow(true);
                return false;
            }

            if (const std::optional<std::string> spriteName = resolvedTemplatePreviewField(*tpl, &templateDatabase_, tpl->name, "sprite"))
            {
                watchSpritePreviewDependencies(*spriteName);
                const std::string cacheKey =
                    std::string("spr:")
                    + sed::text::upperAscii(*spriteName)
                    + "|deps="
                    + detail::sharedPreviewSpriteDependencyRevisionToken(
                        resources_.get(), options_.previewCmp, *spriteName);
                if (const auto it = templatePreviewIconCache_.find(cacheKey); it != templatePreviewIconCache_.end())
                {
                    assignedPreviewIcon = it->second;
                    assignedIcon = true;
                }
                else if (const std::optional<QImage> spriteImage = loadTemplateSpritePreviewImage(resources_.get(), options_.previewCmp, *spriteName))
                {
                    const QIcon icon(makeTemplatePreviewPixmap(*spriteImage, resourcesList_->iconSize()));
                    templatePreviewIconCache_[cacheKey] = icon;
                    assignedPreviewIcon = icon;
                    assignedIcon = true;
                }
            }
            if (!assignedIcon
                && thumbMode != TemplateListModelPreviewMode::none)
            {
                std::optional<std::string> modelName = resolvedTemplatePreviewField(*tpl, &templateDatabase_, tpl->name, "model3d");
                if (!modelName)
                {
                    modelName = resolvedTemplatePreviewField(*tpl, &templateDatabase_, tpl->name, "model3do");
                }
                if (modelName)
                {
                    watchModelPreviewDependencies(*modelName);
                    const QSize iconSize = resourcesList_->iconSize();
                    QIcon icon{};
                    const auto result = detail::loadSharedModelPreviewIcon(
                        templatePreviewIconCache_,
                        modelThumbnailRenderer_,
                        thumbnailResources_,
                        options_.previewProjectType,
                        options_.previewCmp,
                        *modelName,
                        iconSize,
                        thumbMode == TemplateListModelPreviewMode::wireframe,
                        templatePreviewActiveJobs_,
                        [this](const std::string& queuedModelName)
                        {
                            queueTemplateModelPrep(queuedModelName);
                        },
                        icon);
                    if (!refreshCurrentItem())
                    {
                        return false;
                    }
                    watchModelPreviewDependencies(*modelName);
                    switch (result)
                    {
                        case detail::SharedModelPreviewIconLoadResult::assigned:
                            assignedPreviewIcon = icon;
                            assignedIcon = true;
                            break;
                        case detail::SharedModelPreviewIconLoadResult::rendered:
                            assignedPreviewIcon = icon;
                            assignedIcon = true;
                            if (renderedAuthoritativeThumbnail)
                            {
                                *renderedAuthoritativeThumbnail = true;
                            }
                            break;
                        case detail::SharedModelPreviewIconLoadResult::pending:
                            return false;
                        case detail::SharedModelPreviewIconLoadResult::unavailable:
                            finishPendingRow(true);
                            return false;
                    }
                }
            }
        }

        if (!assignedIcon)
        {
            finishPendingRow(true);
            return false;
        }

        if (!refreshCurrentItem())
        {
            return false;
        }

        detail::publishPreviewThumbnailItem(
            resourcesList_,
            item,
            assignedPreviewIcon,
            kPreviewThumbStateRole,
            static_cast<int>(PreviewThumbState::ready));
        templatePreviewQueue_.finish(row);
        return true;
    }

    bool requeueVisibleLoadingTemplatePreviewRows()
    {
        if ((options_.kind != ResourcePickerKind::template_ && options_.kind != ResourcePickerKind::model3do)
            || !resourcesList_
            || templatePreviewQueue_.empty())
        {
            return false;
        }

        int rowBegin = -1;
        int rowEnd = -1;
        if (!visibleMatRowRange(rowBegin, rowEnd))
        {
            return false;
        }

        const auto kThumbTuning = detail::sharedModelThumbLoadTuning();
        const int margin = kThumbTuning.visibleMargin;
        rowBegin = std::max(0, rowBegin - margin);
        rowEnd = std::min(resourcesList_->count() - 1, rowEnd + margin);

        bool repaired = false;
        for (int row = rowBegin; row <= rowEnd; ++row)
        {
            if (!templatePreviewQueue_.hasRow(row))
            {
                continue;
            }
            if (templatePreviewQueue_.isPending(row))
            {
                continue;
            }

            QListWidgetItem* item = resourcesList_->item(row);
            if (!item)
            {
                continue;
            }

            const PreviewThumbState state = static_cast<PreviewThumbState>(
                item->data(kPreviewThumbStateRole).toInt());
            if (state != PreviewThumbState::loading)
            {
                continue;
            }

            repaired = templatePreviewQueue_.markPending(row) || repaired;
        }

        return repaired;
    }

    bool saveCategoryTreeModel()
    {
        if (!categoryTreePath_ || !categoryTreeModel_)
        {
            QMessageBox::warning(
                this,
                QStringLiteral("Save categories failed"),
                QStringLiteral("No editable category location is available. Save the level and try again."));
            return false;
        }

        try
        {
            std::error_code ec;
            const std::filesystem::path parent = categoryTreePath_->parent_path();
            if (!parent.empty())
            {
                std::filesystem::create_directories(parent, ec);
                if (ec)
                {
                    throw std::runtime_error("Failed to create category directory: " + parent.string());
                }
            }
            assets::saveToJLH(*categoryTreeModel_, *categoryTreePath_);
            return true;
        }
        catch (const std::exception& ex)
        {
            QMessageBox::warning(
                this,
                QStringLiteral("Save categories failed"),
                QStringLiteral("Couldn't save category files.\n%1").arg(QString::fromUtf8(ex.what())));
            return false;
        }
    }

    QTreeWidgetItem* findCategoryTreeItemByPath(const QString& encodedPath) const
    {
        if (!sources_)
        {
            return nullptr;
        }

        const QList<QTreeWidgetItem*> matches =
            sources_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive);
        for (QTreeWidgetItem* item : matches)
        {
            if (item && item->data(0, kCategoryPathRole).toString() == encodedPath)
            {
                return item;
            }
        }
        return nullptr;
    }

    QTreeWidgetItem* createCategoryTreeItem(
        QTreeWidgetItem* parent,
        const assets::JlhNode& node,
        const std::vector<std::size_t>& path)
    {
        if (!parent)
        {
            return nullptr;
        }

        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, QString::fromStdString(node.label));
        item->setChildIndicatorPolicy(
            visibleCategoryChildCount(node) == 0
                ? QTreeWidgetItem::DontShowIndicatorWhenChildless
                : QTreeWidgetItem::ShowIndicator);
        item->setData(0, kCategoryPathRole, encodeCategoryPath(path));
        item->setData(0, kCategoryHasListRole, assignableCategoryList(node) != nullptr);
        item->setData(0, kCategoryRootRole, false);
        item->setFlags(item->flags() | Qt::ItemIsEditable);

        QStringList names{};
        appendTemplateCategoryEntriesInSourceOrder(names, node);
        if (!names.isEmpty())
        {
            item->setData(0, kTemplateCategoryNamesRole, names);
        }
        return item;
    }

    void beginCategoryTreeItemEdit(
        QTreeWidgetItem* item,
        const QString& encodedPath,
        const QString& parentPath,
        const CategoryEditState state,
        const QString& originalLabel)
    {
        if (!sources_ || !item)
        {
            return;
        }

        pendingCategoryEditState_ = state;
        pendingCategoryEditPath_ = encodedPath;
        pendingCategoryEditParentPath_ = parentPath;
        pendingCategoryEditOriginalLabel_ = originalLabel;

        sources_->setCurrentItem(item);
        sources_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        sources_->editItem(item, 0);
    }

    void finalizePendingCategoryEdit()
    {
        if (pendingCategoryEditState_ == CategoryEditState::none)
        {
            return;
        }

        const CategoryEditState editState = pendingCategoryEditState_;
        const QString encodedPath = pendingCategoryEditPath_;
        const QString parentPath = pendingCategoryEditParentPath_;
        const QString originalLabel = pendingCategoryEditOriginalLabel_;
        pendingCategoryEditState_ = CategoryEditState::none;
        pendingCategoryEditPath_.clear();
        pendingCategoryEditParentPath_.clear();
        pendingCategoryEditOriginalLabel_.clear();

        QTreeWidgetItem* item = findCategoryTreeItemByPath(encodedPath);
        const QString label = item ? item->text(0).trimmed() : QString{};
        const std::vector<std::size_t> path = decodeCategoryPath(encodedPath);

        if (editState == CategoryEditState::creating && label.isEmpty())
        {
            std::vector<std::size_t> siblingsPath = path;
            const std::size_t index = siblingsPath.empty() ? std::size_t{} : siblingsPath.back();
            if (!siblingsPath.empty())
            {
                siblingsPath.pop_back();
            }
            if (auto* siblings = categorySiblingsAt(*categoryTreeModel_, siblingsPath))
            {
                if (index < siblings->size())
                {
                    siblings->erase(siblings->begin() + static_cast<std::ptrdiff_t>(index));
                }
            }
            reloadCategoryTreeUiPreserveSelection(parentPath);
            return;
        }

        if (editState == CategoryEditState::renaming && label.isEmpty())
        {
            if (assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, path))
            {
                node->label = originalLabel.toStdString();
            }
            reloadCategoryTreeUiPreserveSelection(encodedPath);
            return;
        }

        if (assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, path))
        {
            node->label = label.toStdString();
            if (saveCategoryTreeModel())
            {
                reloadCategoryTreeUiPreserveSelection(encodedPath);
            }
            else
            {
                reloadCategoryTreeUiPreserveSelection(parentPath);
            }
        }
    }

    void reloadCategoryTreeUiPreserveSelection(const QString& selectedPath = QString{})
    {
        const QString previousResource = currentResourceName();
        rebuildSources();
        if (!selectedPath.isEmpty() && sources_)
        {
            const QList<QTreeWidgetItem*> matches = sources_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive);
            for (QTreeWidgetItem* item : matches)
            {
                if (item && item->data(0, kCategoryPathRole).toString() == selectedPath)
                {
                    sources_->setCurrentItem(item);
                    break;
                }
            }
        }
        refreshResourcesForSource();
        if (!previousResource.isEmpty())
        {
            selectResource(previousResource);
        }
    }

    assets::JllList* currentEditableCategoryList(QString* encodedPath = nullptr)
    {
        if (!sources_ || !categoryTreeModel_ || !sources_->currentItem())
        {
            return nullptr;
        }

        QTreeWidgetItem* item = sources_->currentItem();
        const QString pathText = item->data(0, kCategoryPathRole).toString();
        if (encodedPath)
        {
            *encodedPath = pathText;
        }

        if (assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, decodeCategoryPath(pathText)))
        {
            if (node->list)
            {
                auto list = std::const_pointer_cast<assets::JllList>(node->list);
                return list.get();
            }

            for (assets::JlhNode& child : node->children)
            {
                if (child.list
                    && QString::fromStdString(child.label).compare(QStringLiteral("General"), Qt::CaseInsensitive) == 0)
                {
                    auto list = std::const_pointer_cast<assets::JllList>(child.list);
                    return list.get();
                }
            }
        }
        return nullptr;
    }

    QStringList selectedResourceNames() const
    {
        QStringList names{};
        if (!resourcesList_)
        {
            return names;
        }

        QList<QListWidgetItem*> items = resourcesList_->selectedItems();
        if (items.isEmpty())
        {
            if (QListWidgetItem* current = resourcesList_->currentItem())
            {
                items.push_back(current);
            }
        }

        for (const QListWidgetItem* item : items)
        {
            if (!item)
            {
                continue;
            }

            const QVariant actual = item->data(kActualNameRole);
            const QString name = actual.isValid() ? actual.toString().trimmed() : item->text().trimmed();
            if (!name.isEmpty())
            {
                names.push_back(name);
            }
        }

        names.removeDuplicates();
        return names;
    }

    bool mutateCategoryMembership(const QString& encodedPath, const QStringList& names, const bool remove)
    {
        if (!categoryTreeModel_ || names.isEmpty())
        {
            return false;
        }

        assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, decodeCategoryPath(encodedPath));
        if (!node)
        {
            return false;
        }

        return detail::mutateCategoryMembership(
            *node,
            names,
            remove,
            [this]()
            {
                return makeCategoryListKey(*categoryTreeModel_, QStringLiteral("category"));
            });
    }

    bool moveCategoryMembership(
        const QString& sourcePath,
        const QString& targetPath,
        const QStringList& names)
    {
        if (!categoryTreeModel_ || names.isEmpty())
        {
            return false;
        }

        assets::JlhNode* source = categoryNodeAt(*categoryTreeModel_, decodeCategoryPath(sourcePath));
        assets::JlhNode* target = categoryNodeAt(*categoryTreeModel_, decodeCategoryPath(targetPath));
        if (!source || !target)
        {
            return false;
        }

        return detail::moveCategoryMembership(
            *source,
            *target,
            names,
            [this]()
            {
                return makeCategoryListKey(*categoryTreeModel_, QStringLiteral("category"));
            });
    }

    void refreshCategoryTreeItemData(const QString& encodedPath)
    {
        if (!sources_ || !categoryTreeModel_ || encodedPath.isEmpty())
        {
            return;
        }

        QTreeWidgetItem* item = findCategoryTreeItemByPath(encodedPath);
        const assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, decodeCategoryPath(encodedPath));
        if (!item || !node)
        {
            return;
        }

        item->setData(0, kCategoryHasListRole, assignableCategoryList(*node) != nullptr);
        QStringList names{};
        appendTemplateCategoryEntriesInSourceOrder(names, *node);
        item->setData(0, kTemplateCategoryNamesRole, names);
    }

    bool applyCategoryMembership(const QString& encodedPath, const QStringList& names, const bool remove)
    {
        if (!categoryTreeModel_)
        {
            return false;
        }

        if (detail::commitCategoryMembershipMutation(
                *categoryTreeModel_,
                [this, &encodedPath, &names, remove]()
                {
                    return mutateCategoryMembership(encodedPath, names, remove);
                },
                [this]()
                {
                    return saveCategoryTreeModel();
                }))
        {
            reloadCategoryTreeUiPreserveSelection(encodedPath);
            return true;
        }
        return false;
    }

    void populateAssignableCategoryMenu(
        QMenu* parentMenu,
        const QStringList& names,
        std::unordered_map<QAction*, QString>& actionPaths) const
    {
        if (!parentMenu || !categoryTreeModel_ || names.isEmpty())
        {
            return;
        }

        std::function<void(QMenu*, const assets::JlhNode&, const std::vector<std::size_t>&)> addNode =
            [&](QMenu* menu, const assets::JlhNode& node, const std::vector<std::size_t>& path)
        {
            if (!menu)
            {
                return;
            }

            const QString label = QString::fromStdString(node.label).trimmed();
            if (label.isEmpty() || isInternalGeneralCategoryNode(node))
            {
                return;
            }

            const QString encodedPath = encodeCategoryPath(path);
            const bool hasChildren = !node.children.empty();
            if (hasChildren)
            {
                auto* submenu = new ui::shortcuts::KeySequenceDisplayMenu(label, menu);
                QAction* addHereAction = submenu->addAction(QStringLiteral("Add Here"));
                actionPaths.emplace(addHereAction, encodedPath);

                bool enabled = false;
                if (const assets::JllList* list = assignableCategoryList(node))
                {
                    for (const QString& name : names)
                    {
                        if (!categoryListContainsEntry(*list, name))
                        {
                            enabled = true;
                            break;
                        }
                    }
                }
                else
                {
                    enabled = true;
                }
                addHereAction->setEnabled(enabled);

                for (std::size_t i = 0; i < node.children.size(); ++i)
                {
                    std::vector<std::size_t> childPath = path;
                    childPath.push_back(i);
                    addNode(submenu, node.children[i], childPath);
                }
                menu->addMenu(submenu);
                return;
            }

            QAction* action = menu->addAction(label);
            actionPaths.emplace(action, encodedPath);

            bool enabled = false;
            if (const assets::JllList* list = assignableCategoryList(node))
            {
                for (const QString& name : names)
                {
                    if (!categoryListContainsEntry(*list, name))
                    {
                        enabled = true;
                        break;
                    }
                }
            }
            else
            {
                enabled = true;
            }
            action->setEnabled(enabled);
        };

        for (std::size_t i = 0; i < categoryTreeModel_->roots.size(); ++i)
        {
            addNode(parentMenu, categoryTreeModel_->roots[i], std::vector<std::size_t>{ i });
        }
    }

    bool currentCategoryMoveSourcePath(QString* encodedPath = nullptr) const
    {
        if (!sources_ || !sources_->currentItem())
        {
            return false;
        }

        QTreeWidgetItem* current = sources_->currentItem();
        if (current->data(0, kCategoryRootRole).toBool())
        {
            return false;
        }

        const QString path = current->data(0, kCategoryPathRole).toString();
        if (path.isEmpty())
        {
            return false;
        }
        if (encodedPath)
        {
            *encodedPath = path;
        }
        return true;
    }

    Qt::DropAction resolveCategoryDropAction(
        QTreeWidgetItem* item,
        const QStringList& names,
        const Qt::KeyboardModifiers modifiers) const
    {
        QString sourcePath{};
        if ((modifiers & Qt::ControlModifier) != 0)
        {
            (void)currentCategoryMoveSourcePath(&sourcePath);
        }
        const Qt::DropAction action = detail::resolveCategoryMembershipDropAction(
            item && item->data(0, kCategoryRootRole).toBool(),
            item ? item->data(0, kCategoryPathRole).toString() : QString{},
            sourcePath,
            names,
            modifiers);
        if (action != Qt::MoveAction || !categoryTreeModel_ || !item)
        {
            return action;
        }

        const assets::JlhNode* source = categoryNodeAt(*categoryTreeModel_, decodeCategoryPath(sourcePath));
        const assets::JlhNode* target = categoryNodeAt(
            *categoryTreeModel_,
            decodeCategoryPath(item->data(0, kCategoryPathRole).toString()));
        return source && target && detail::categoryMembershipNodesShareStorage(*source, *target)
            ? Qt::IgnoreAction
            : action;
    }

    bool handleCategoryDrop(QTreeWidgetItem* item, const QStringList& names, const Qt::DropAction action)
    {
        if (!item || names.isEmpty())
        {
            return false;
        }

        const bool root = item->data(0, kCategoryRootRole).toBool();
        const QString encodedPath = item->data(0, kCategoryPathRole).toString();
        if (root || encodedPath.isEmpty())
        {
            return false;
        }

        if (!categoryTreeModel_)
        {
            return false;
        }

        QString sourcePath{};
        const bool move = action == Qt::MoveAction && currentCategoryMoveSourcePath(&sourcePath) && sourcePath != encodedPath;
        if (!detail::commitCategoryMembershipMutation(
                *categoryTreeModel_,
                [this, &encodedPath, &sourcePath, &names, move]()
                {
                    return move
                        ? moveCategoryMembership(sourcePath, encodedPath, names)
                        : mutateCategoryMembership(encodedPath, names, false);
                },
                [this]()
                {
                    return saveCategoryTreeModel();
                }))
        {
            return false;
        }

        refreshCategoryTreeItemData(encodedPath);
        if (move)
        {
            refreshCategoryTreeItemData(sourcePath);
            refreshResourcesForSource();
        }
        return true;
    }

    void convertCategoryLeafToBranch(assets::JlhNode& node)
    {
        if (!node.list)
        {
            return;
        }

        assets::JlhNode retained{};
        retained.label = QStringLiteral("General").toStdString();
        retained.list = node.list;
        node.list.reset();
        node.children.insert(node.children.begin(), std::move(retained));
    }

    void showResourceMembershipContextMenu(const QPoint& pos)
    {
        if (!resourcesList_ || !categoryTreeModel_)
        {
            return;
        }

        auto* assignableList = static_cast<detail::CategoryAssignableListWidget*>(resourcesList_);
        ui::shortcuts::KeySequenceDisplayMenu menu(this);
        QAction* toggleMultiSelectAction = menu.addAction(
            assignableList->categoryMultiSelectMode()
                ? QStringLiteral("Disable Multi Select")
                : QStringLiteral("Enable Multi Select"));

        const QStringList selectedNames = selectedResourceNames();
        QString categoryPath{};
        assets::JllList* list = currentEditableCategoryList(&categoryPath);
        QAction* addAction = nullptr;
        QAction* removeAction = nullptr;
        if (!selectedNames.isEmpty() && list)
        {
            menu.addSeparator();
            bool anyMissing = false;
            bool anyPresent = false;
            for (const QString& name : selectedNames)
            {
                const bool contains = categoryListContainsEntry(*list, name);
                anyMissing = anyMissing || !contains;
                anyPresent = anyPresent || contains;
            }

            addAction = menu.addAction(selectedNames.size() > 1
                                           ? QStringLiteral("Add Selected Items To Current Category")
                                           : QStringLiteral("Add To Current Category"));
            removeAction = menu.addAction(selectedNames.size() > 1
                                              ? QStringLiteral("Remove Selected Items From Current Category")
                                              : QStringLiteral("Remove From Current Category"));
            addAction->setEnabled(anyMissing);
            removeAction->setEnabled(anyPresent);
        }

        std::unordered_map<QAction*, QString> addTargets{};
        QMenu* addMenu = nullptr;
        if (!selectedNames.isEmpty())
        {
            if (addAction || removeAction)
            {
                menu.addSeparator();
            }
            addMenu = ui::shortcuts::addKeySequenceDisplayMenu(
                &menu, QStringLiteral("Add To Category"));
            populateAssignableCategoryMenu(addMenu, selectedNames, addTargets);
            if (addMenu->isEmpty())
            {
                addMenu->setEnabled(false);
            }
        }

        QAction* chosen = menu.exec(resourcesList_->viewport()->mapToGlobal(pos));
        if (!chosen)
        {
            return;
        }

        if (chosen == toggleMultiSelectAction)
        {
            if (assignableList->categoryMultiSelectMode())
            {
                assignableList->disableCategoryMultiSelectPreserveSelection();
            }
            else
            {
                assignableList->setCategoryMultiSelectMode(true);
            }
            return;
        }

        if (chosen == addAction)
        {
            applyCategoryMembership(categoryPath, selectedNames, false);
            return;
        }

        if (chosen == removeAction)
        {
            applyCategoryMembership(categoryPath, selectedNames, true);
            return;
        }

        if (const auto it = addTargets.find(chosen); it != addTargets.end())
        {
            applyCategoryMembership(it->second, selectedNames, false);
            return;
        }

    }

    void showCategoryContextMenu(const QPoint& pos)
    {
        if (!sources_ || !categoryTreeModel_)
        {
            return;
        }

        QTreeWidgetItem* item = sources_->itemAt(pos);
        const bool clickedEmpty = (item == nullptr);
        const bool root = !item || item->data(0, kCategoryRootRole).toBool();
        const QString encodedPath = item ? item->data(0, kCategoryPathRole).toString() : QString{};
        const std::vector<std::size_t> path = decodeCategoryPath(encodedPath);
        const assets::JlhNode* node = (!root && categoryTreeModel_) ? categoryNodeAt(*categoryTreeModel_, path) : nullptr;
        const bool hasList = node && assignableCategoryList(*node);

        ui::shortcuts::KeySequenceDisplayMenu menu(this);
        QAction* addCategoryAction = menu.addAction((clickedEmpty || root) ? QStringLiteral("Add Category...") : QStringLiteral("Add Sibling Category..."));
        QAction* addSubcategoryAction = nullptr;
        QAction* addLeafAction = nullptr;
        QAction* renameAction = nullptr;
        QAction* deleteAction = nullptr;
        QAction* assignAction = nullptr;
        QAction* removeAction = nullptr;

        if (item && !root)
        {
            addSubcategoryAction = menu.addAction(QStringLiteral("Add Subcategory..."));
            if (!hasList)
            {
                addLeafAction = menu.addAction(QStringLiteral("Add Leaf Category..."));
            }
        }
        if (item && !root)
        {
            renameAction = menu.addAction(QStringLiteral("Rename..."));
            deleteAction = menu.addAction(QStringLiteral("Delete..."));
        }
        const QStringList selectedNames = selectedResourceNames();
        if (item && !root && !selectedNames.isEmpty())
        {
            bool anyMissing = false;
            bool anyPresent = false;
            if (const assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, path))
            {
                const assets::JllList* list = assignableCategoryList(*node);
                if (list)
                {
                    for (const QString& name : selectedNames)
                    {
                        const bool contains = categoryListContainsEntry(*list, name);
                        anyMissing = anyMissing || !contains;
                        anyPresent = anyPresent || contains;
                    }
                }
                else
                {
                    anyMissing = true;
                }
            }
            menu.addSeparator();
            assignAction = menu.addAction(selectedNames.size() > 1
                                              ? QStringLiteral("Add Selected Items Here")
                                              : QStringLiteral("Add Selected Item Here"));
            assignAction->setEnabled(anyMissing);
            if (hasList)
            {
                removeAction = menu.addAction(selectedNames.size() > 1
                                                  ? QStringLiteral("Remove Selected Items From Here")
                                                  : QStringLiteral("Remove Selected Item From Here"));
                removeAction->setEnabled(anyPresent);
            }
        }

        QAction* chosen = menu.exec(sources_->viewport()->mapToGlobal(pos));
        if (!chosen)
        {
            return;
        }

        if (chosen == addCategoryAction)
        {
            const std::vector<std::size_t> parentPath = (clickedEmpty || root) ? std::vector<std::size_t>{} : [&]()
            {
                std::vector<std::size_t> p = path;
                if (!p.empty())
                {
                    p.pop_back();
                }
                return p;
            }();
            if (auto* siblings = categorySiblingsAt(*categoryTreeModel_, parentPath))
            {
                siblings->push_back(assets::JlhNode{ "", {}, {} });
                const std::vector<std::size_t> newPath = [&]()
                {
                    std::vector<std::size_t> p = parentPath;
                    p.push_back(siblings->size() - 1);
                    return p;
                }();
                QTreeWidgetItem* parentItem =
                    (clickedEmpty || root) ? sources_->invisibleRootItem()
                         : (item->parent() ? item->parent() : sources_->invisibleRootItem());
                if (QTreeWidgetItem* created = createCategoryTreeItem(parentItem ? parentItem : item, siblings->back(), newPath))
                {
                    beginCategoryTreeItemEdit(
                        created,
                        encodeCategoryPath(newPath),
                        encodeCategoryPath(parentPath),
                        CategoryEditState::creating,
                        QString{});
                }
            }
            return;
        }

        if (chosen == addSubcategoryAction)
        {
            if (assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, path))
            {
                if (node->list)
                {
                    convertCategoryLeafToBranch(*node);
                }
                node->children.push_back(assets::JlhNode{ "", {}, {} });
                std::vector<std::size_t> newPath = path;
                newPath.push_back(node->children.size() - 1);
                if (QTreeWidgetItem* created = createCategoryTreeItem(item, node->children.back(), newPath))
                {
                    item->setExpanded(true);
                    beginCategoryTreeItemEdit(
                        created,
                        encodeCategoryPath(newPath),
                        encodedPath,
                        CategoryEditState::creating,
                        QString{});
                }
            }
            return;
        }

        if (chosen == addLeafAction)
        {
            if (root)
            {
                auto list = std::make_shared<assets::JllList>();
                list->name = makeCategoryListKey(*categoryTreeModel_, QStringLiteral("category"));
                categoryTreeModel_->roots.push_back(assets::JlhNode{ "", list, {} });
                const std::vector<std::size_t> newPath{ categoryTreeModel_->roots.size() - 1 };
                if (QTreeWidgetItem* created = createCategoryTreeItem(sources_->invisibleRootItem(), categoryTreeModel_->roots.back(), newPath))
                {
                    beginCategoryTreeItemEdit(
                        created,
                        encodeCategoryPath(newPath),
                        QString{},
                        CategoryEditState::creating,
                        QString{});
                }
                return;
            }

            if (assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, path))
            {
                auto list = std::make_shared<assets::JllList>();
                list->name = makeCategoryListKey(*categoryTreeModel_, QStringLiteral("category"));
                node->children.push_back(assets::JlhNode{ "", list, {} });
                std::vector<std::size_t> newPath = path;
                newPath.push_back(node->children.size() - 1);
                if (QTreeWidgetItem* created = createCategoryTreeItem(item, node->children.back(), newPath))
                {
                    item->setExpanded(true);
                    beginCategoryTreeItemEdit(
                        created,
                        encodeCategoryPath(newPath),
                        encodedPath,
                        CategoryEditState::creating,
                        QString{});
                }
            }
            return;
        }

        if (chosen == renameAction)
        {
            assets::JlhNode* node = categoryNodeAt(*categoryTreeModel_, path);
            if (!node)
            {
                return;
            }
            std::vector<std::size_t> parentPath = path;
            if (!parentPath.empty())
            {
                parentPath.pop_back();
            }
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            beginCategoryTreeItemEdit(
                item,
                encodedPath,
                encodeCategoryPath(parentPath),
                CategoryEditState::renaming,
                QString::fromStdString(node->label));
            return;
        }

        if (chosen == deleteAction)
        {
            QMessageBox confirmation(QMessageBox::Question,
                QStringLiteral("Delete category"),
                QStringLiteral("Delete \"%1\" and its contents?").arg(item->text(0)),
                QMessageBox::Yes | QMessageBox::No,
                this);
            confirmation.setObjectName(QStringLiteral("resourcePickerDeleteCategoryConfirmation"));
            confirmation.setTextFormat(Qt::PlainText);
            confirmation.setDefaultButton(QMessageBox::No);
            confirmation.setEscapeButton(QMessageBox::No);
            confirmation.setProperty(
                kMessageBoxLoggingDestructiveProperty, true);
            confirmation.setProperty(kMessageBoxLoggingContextProperty,
                QStringLiteral("Resource category: %1")
                    .arg(item->text(0)));
            if (confirmation.exec() != QMessageBox::Yes)
            {
                return;
            }

            std::vector<std::size_t> parentPath = path;
            const std::size_t index = parentPath.empty() ? std::size_t{} : parentPath.back();
            if (!parentPath.empty())
            {
                parentPath.pop_back();
            }
            if (auto* siblings = categorySiblingsAt(*categoryTreeModel_, parentPath))
            {
                if (index < siblings->size())
                {
                    siblings->erase(siblings->begin() + static_cast<std::ptrdiff_t>(index));
                    if (saveCategoryTreeModel())
                    {
                        reloadCategoryTreeUiPreserveSelection(encodeCategoryPath(parentPath));
                    }
                }
            }
            return;
        }

        if (chosen == assignAction)
        {
            applyCategoryMembership(encodedPath, selectedNames, false);
            return;
        }

        if (chosen == removeAction)
        {
            applyCategoryMembership(encodedPath, selectedNames, true);
        }
    }

    void queueTemplateModelPrep(const std::string& modelName)
    {
        const QSize iconSize = resourcesList_
            ? resourcesList_->iconSize()
            : QSize(96, 96);
        const bool wireframe =
            loadTemplateListModelPreviewMode()
            == TemplateListModelPreviewMode::wireframe;
        (void)detail::queueSharedPreviewModelBoxPrep(
            this,
            previewResourcesForResource(modelName),
            modelName,
            options_.previewCmp,
            iconSize,
            wireframe,
            templatePreviewPrepInFlight_,
            templatePreviewActiveJobs_,
            [this, modelName](const bool current)
            {
                scheduleTemplatePreviewLoad(0);
                if (current
                    && options_.kind == ResourcePickerKind::model3do
                    && currentResourceName().compare(QString::fromStdString(modelName), Qt::CaseInsensitive) == 0)
                {
                    updatePreview();
                }
            });
    }

    void watchModelPreviewDependencies(const std::string_view modelName)
    {
        if (!previewResourceDiskWatcher_)
        {
            return;
        }

        previewResourceDiskWatcher_->watchResource(modelName, ".3do");
        previewResourceDiskWatcher_->watchResource(options_.previewCmp, ".cmp");
        std::shared_ptr<const assets::Model3do> model{};
        if (detail::findCachedSharedPreviewModel(
                previewResourcesForResource(modelName).get(), modelName, model)
            && model)
        {
            for (const std::string& material : model->materials)
            {
                previewResourceDiskWatcher_->watchResource(material, ".mat");
            }
        }
    }

    void watchSpritePreviewDependencies(const std::string_view spriteName)
    {
        if (!previewResourceDiskWatcher_)
        {
            return;
        }

        previewResourceDiskWatcher_->watchResource(spriteName, ".spr");
        previewResourceDiskWatcher_->watchResource(options_.previewCmp, ".cmp");
        if (const std::shared_ptr<const assets::SprFile> sprite =
                detail::loadSharedPreviewSprite(previewResourcesForResource(spriteName).get(), spriteName))
        {
            previewResourceDiskWatcher_->watchResource(sprite->material, ".mat");
        }
    }

    void processTemplatePreviewLoadBatch()
    {
        if (closing_
            || (filterRefreshTimer_ && filterRefreshTimer_->isActive()))
        {
            templatePreviewLoadTimer_.stop();
            return;
        }
        setProperty("resourcePickerTemplatePreviewBatchSerialForTest",
            QVariant::fromValue<qulonglong>(
                property("resourcePickerTemplatePreviewBatchSerialForTest")
                    .toULongLong() + 1));
        (void)requeueVisibleLoadingTemplatePreviewRows();
        if (templatePreviewPublicationPacer_.interactionActive())
        {
            setProperty(
                "resourcePickerTemplatePreviewDeferredForInteractionForTest",
                true);
            scheduleTemplatePreviewLoad(
                templatePreviewPublicationPacer_.delayUntilQuiet());
            return;
        }
        setProperty(
            "resourcePickerTemplatePreviewDeferredForInteractionForTest",
            false);
        if (templatePreviewLoadInProgress_)
        {
            scheduleTemplatePreviewLoad(8);
            return;
        }
        if ((options_.kind != ResourcePickerKind::template_ && options_.kind != ResourcePickerKind::model3do)
            || !resourcesList_
            || templatePreviewQueue_.pendingCount() <= 0)
        {
            templatePreviewLoadTimer_.stop();
            return;
        }

        struct TemplatePreviewBatchScope final
        {
            bool& flag;
            explicit TemplatePreviewBatchScope(bool& value)
                : flag(value)
            {
                flag = true;
            }
            ~TemplatePreviewBatchScope()
            {
                flag = false;
            }
        } scope(templatePreviewLoadInProgress_);

        const int itemCount = resourcesList_->count();
        if (itemCount <= 0)
        {
            clearTemplatePreviewPending();
            return;
        }

        const auto kThumbTuning = detail::sharedModelThumbLoadTuning();
        const int visibleBatchCount = kThumbTuning.visibleBatchCount;
        const int offscreenBatchCount = kThumbTuning.offscreenBatchCount;
        const std::uint64_t batchGeneration = resourceListGeneration_;
        int loaded = 0;
        int processed = 0;
        bool visiblePending = false;
        bool renderedAuthoritativeThumbnail = false;
        QElapsedTimer batchTimer{};
        batchTimer.start();

        int rowBegin = -1;
        int rowEnd = -1;
        if (visibleMatRowRange(rowBegin, rowEnd))
        {
            const int margin = kThumbTuning.visibleMargin;
            rowBegin = std::max(0, rowBegin - margin);
            rowEnd = std::min(itemCount - 1, rowEnd + margin);
            const std::vector<int> priorityRows =
                detail::previewThumbnailPriorityRows(
                    rowBegin,
                    rowEnd,
                    resourcesList_->currentRow());
            for (const int row : priorityRows)
            {
                if (templatePreviewQueue_.isPending(row))
                {
                    visiblePending = true;
                }
                if (!templatePreviewQueue_.isPending(row)
                    || renderedAuthoritativeThumbnail
                    || !detail::previewThumbnailBatchCanContinue(
                        processed,
                        visibleBatchCount,
                        batchTimer.nsecsElapsed()))
                {
                    continue;
                }
                bool renderedThisRow = false;
                ++processed;
                if (loadTemplatePreviewForRow(row, &renderedThisRow))
                {
                    ++loaded;
                }
                renderedAuthoritativeThumbnail =
                    renderedAuthoritativeThumbnail || renderedThisRow;
                if (batchGeneration != resourceListGeneration_)
                {
                    return;
                }
            }
        }

        int scanned = 0;
        while (!visiblePending
               && loaded < offscreenBatchCount
               && scanned < itemCount
               && templatePreviewQueue_.pendingCount() > 0)
        {
            const int row = templatePreviewQueue_.nextScanRow(itemCount);
            if (loadTemplatePreviewForRow(row))
            {
                ++loaded;
            }
            if (batchGeneration != resourceListGeneration_)
            {
                return;
            }
            ++scanned;
        }

        if (visiblePending && templatePreviewQueue_.pendingCount() > 0)
        {
            if (detail::previewThumbnailQueueNeedsPolling(
                    true, templatePreviewActiveJobs_))
            {
                scheduleTemplatePreviewLoad(kThumbTuning.visibleDelayMs);
            }
            else
            {
                templatePreviewLoadTimer_.stop();
            }
        }
        else
        {
            templatePreviewLoadTimer_.stop();
        }
    }

    void syncModel3doDiskWatches()
    {
        if (!previewResourceDiskWatcher_
            || (options_.kind != ResourcePickerKind::model3do
                && options_.kind != ResourcePickerKind::template_))
        {
            return;
        }

        previewResourceDiskWatcher_->setResourceLocator(resources_.get());
        previewResourceDiskWatcher_->clearDependencies();
        previewResourceDiskWatcher_->watchResource(options_.previewCmp, ".cmp");

        // Raw 3DO and template lists both add model/MAT dependencies when a
        // row becomes visible or selected. Registering every raw-model row
        // here synchronously resolved thousands of disk/GOB revisions before
        // the first thumbnail timer could run. Revision-keyed caches still
        // reject stale off-screen icons when those rows become visible later.
    }

    void handleModel3doDiskChanged()
    {
        if (options_.kind != ResourcePickerKind::model3do
            && options_.kind != ResourcePickerKind::template_)
        {
            return;
        }

        selectedSourcePreviewResources_.clear();
        clearTemplatePreviewPending();
        templatePreviewIconCache_.clear();
        detail::clearSharedPreviewAssetCaches();
        modelThumbnailRenderer_.reset();
        sourceResourcesCache_.clear();
        diskResourceNames_.clear();
        diskResourceNamesResolved_ = false;

        if (thingPreview_)
        {
            thingPreview_->invalidateResourceCaches();
        }

        refreshResourcesForSource();
        updatePreview();
        kickVisibleThumbLoads(0);
    }

    void rebuildSources()
    {
        if (!sources_)
        {
            return;
        }

        sourceResourcesCache_.clear();

        if (categoryFileNameForPickerKind(options_.kind, options_.previewProjectType).has_value())
        {
            sources_->clear();
            sourcesData_.clear();
            categoryTreePath_ = editableCategoryTreePath(
                options_.kind,
                options_.previewProjectType,
                options_.projectDirectory);
            categoryTreeModel_ = loadTemplateCategoryTree(
                *resources_,
                options_.kind,
                options_.previewProjectType,
                options_.projectDirectory);

            if (!categoryTreeModel_ && options_.kind == ResourcePickerKind::wav)
            {
                std::vector<QString> allWavs = collectResourcesForSource(
                    *resources_,
                    spec_,
                    SourceKind::all,
                    -1,
                    options_.preferredDiskRoot);
                sourceResourcesCache_[resourceSourceCacheKey(SourceKind::all, -1)] = allWavs;
                categoryTreeModel_ = makeDefaultWavCategoryTree(allWavs);
            }
            auto* item = new QTreeWidgetItem(sources_);
            item->setText(0, QStringLiteral("All"));
            item->setData(0, kSourceKindRole, static_cast<int>(SourceKind::all));
            item->setData(0, kSourceIndexRole, -1);
            item->setData(0, kCategoryRootRole, true);
            item->setData(0, kCategoryPathRole, QString{});
            item->setData(0, kCategoryHasListRole, false);
            if (categoryTreeModel_)
            {
                std::function<void(QTreeWidgetItem*, const assets::JlhNode&, std::vector<std::size_t>)> addNode =
                    [&](QTreeWidgetItem* parent, const assets::JlhNode& node, std::vector<std::size_t> path)
                    {
                        auto* child = new QTreeWidgetItem(parent);
                        child->setText(0, QString::fromStdString(node.label));
                        child->setChildIndicatorPolicy(
                            visibleCategoryChildCount(node) == 0
                                ? QTreeWidgetItem::DontShowIndicatorWhenChildless
                                : QTreeWidgetItem::ShowIndicator);
                        child->setData(0, kCategoryPathRole, encodeCategoryPath(path));
                        child->setData(0, kCategoryHasListRole, assignableCategoryList(node) != nullptr);
                        child->setData(0, kCategoryRootRole, false);
                        QStringList names{};
                        appendTemplateCategoryEntriesInSourceOrder(names, node);
                        if (!names.isEmpty())
                        {
                            child->setData(0, kTemplateCategoryNamesRole, names);
                        }
                        for (std::size_t i = 0; i < node.children.size(); ++i)
                        {
                            if (isInternalGeneralCategoryNode(node.children[i]))
                            {
                                continue;
                            }
                            std::vector<std::size_t> childPath = path;
                            childPath.push_back(i);
                            addNode(child, node.children[i], std::move(childPath));
                        }
                    };

                for (std::size_t i = 0; i < categoryTreeModel_->roots.size(); ++i)
                {
                    addNode(sources_->invisibleRootItem(), categoryTreeModel_->roots[i], std::vector<std::size_t>{ i });
                }
                sources_->expandAll();
            }
            sources_->setCurrentItem(item);
            return;
        }

        categoryTreeModel_.reset();
        categoryTreePath_.reset();

        const int prevKind = sources_->currentItem() ? sources_->currentItem()->data(0, kSourceKindRole).toInt() : static_cast<int>(SourceKind::all);
        const int prevIndex = sources_->currentItem() ? sources_->currentItem()->data(0, kSourceIndexRole).toInt() : -1;

        sources_->clear();
        sourcesData_ = collectSources(*resources_, spec_, options_.preferredDiskRoot);

        QTreeWidgetItem* best = nullptr;
        for (const ResourceSource& source : sourcesData_)
        {
            auto* item = new QTreeWidgetItem(sources_);
            item->setText(0, source.label);
            item->setData(0, kSourceKindRole, static_cast<int>(source.kind));
            item->setData(0, kSourceIndexRole, source.index);
            if (static_cast<int>(source.kind) == prevKind && source.index == prevIndex)
            {
                best = item;
            }
        }

        if (!best && sources_->topLevelItemCount() > 0)
        {
            best = sources_->topLevelItem(0);
        }
        if (best)
        {
            sources_->setCurrentItem(best);
        }
    }

    QStringList selectedCategoryNames() const
    {
        if (!sources_ || !sources_->currentItem())
        {
            return {};
        }

        QStringList names{};
        std::function<void(QTreeWidgetItem*)> collect = [&](QTreeWidgetItem* item)
        {
            if (!item)
            {
                return;
            }

            const QStringList directNames = item->data(0, kTemplateCategoryNamesRole).toStringList();
            for (const QString& name : directNames)
            {
                if (!names.contains(name, Qt::CaseInsensitive))
                {
                    names.push_back(name);
                }
            }

            for (int i = 0; i < item->childCount(); ++i)
            {
                collect(item->child(i));
            }
        };

        collect(sources_->currentItem());
        return names;
    }

    std::vector<QString> cachedResourcesForCurrentSource()
    {
        const SourceKind sourceKind = currentSourceKind();
        const int sourceIndex = currentSourceIndex();
        const std::string cacheKey = resourceSourceCacheKey(sourceKind, sourceIndex);
        if (const auto it = sourceResourcesCache_.find(cacheKey); it != sourceResourcesCache_.end())
        {
            return it->second;
        }

        std::vector<QString> collected = collectResourcesForSource(
            *resources_,
            spec_,
            sourceKind,
            sourceIndex,
            options_.preferredDiskRoot);
        sourceResourcesCache_[cacheKey] = collected;
        return collected;
    }

    void refreshResourcesForSource()
    {
        if (options_.kind == ResourcePickerKind::template_)
        {
            templateDatabase_.clear();
            if (const std::optional<assets::Templates> loaded = loadTemplatePickerDatabase(*resources_, options_.previewProjectType))
            {
                templateDatabase_ = *loaded;
            }
            mergeTemplateDatabaseFallback(templateDatabase_, options_.templateDatabaseFallback);
            mergeTemplateOverrides(templateDatabase_, options_.templateOverrides);
            resourcesAll_.clear();

            const bool rootCategorySelected =
                sources_
                && sources_->currentItem()
                && sources_->currentItem()->data(0, kCategoryRootRole).toBool();
            const QStringList categoryNames = rootCategorySelected ? QStringList{} : selectedCategoryNames();

            if (!rootCategorySelected && categoryNames.isEmpty())
            {
                commitResourceSourceOrder();
                return;
            }

            if (!categoryNames.isEmpty())
            {
                for (const QString& categoryName : categoryNames)
                {
                    if (!templateDatabase_.find(categoryName.toStdString()))
                    {
                        continue;
                    }

                    if (!options_.allowBaseTemplates && isBaseTemplateName(categoryName))
                    {
                        continue;
                    }

                    bool duplicate = false;
                    for (const QString& existing : resourcesAll_)
                    {
                        if (existing.compare(categoryName, Qt::CaseInsensitive) == 0)
                        {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate)
                    {
                        resourcesAll_.push_back(categoryName);
                    }
                }
            }
            else
            {
                for (std::size_t i = 0; i < templateDatabase_.size(); ++i)
                {
                    if (const ThingTemplate* tpl = templateDatabase_.get(i))
                    {
                        const QString name = QString::fromStdString(tpl->name).trimmed();
                        if (!name.isEmpty()
                            && (options_.allowBaseTemplates || !isBaseTemplateName(name)))
                        {
                            resourcesAll_.push_back(name);
                        }
                    }
                }
            }
            commitResourceSourceOrder();
            return;
        }

        if (categoryFileNameForPickerKind(options_.kind, options_.previewProjectType).has_value())
        {
            resourcesAll_.clear();

            const bool rootCategorySelected =
                sources_
                && sources_->currentItem()
                && sources_->currentItem()->data(0, kCategoryRootRole).toBool();
            const QStringList categoryNames = rootCategorySelected ? QStringList{} : selectedCategoryNames();

            if (!rootCategorySelected && categoryNames.isEmpty())
            {
                commitResourceSourceOrder();
                return;
            }

            if (!categoryNames.isEmpty())
            {
                for (const QString& rawCategoryName : categoryNames)
                {
                    const QString resourceName = ensureDefaultExtension(rawCategoryName.trimmed());
                    if (resourceName.isEmpty())
                    {
                        continue;
                    }

                    bool duplicate = false;
                    for (const QString& existing : resourcesAll_)
                    {
                        if (existing.compare(resourceName, Qt::CaseInsensitive) == 0)
                        {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate)
                    {
                        resourcesAll_.push_back(resourceName);
                    }
                }
            }
            else
            {
                resourcesAll_ = cachedResourcesForCurrentSource();
            }

            commitResourceSourceOrder();
            return;
        }

        resourcesAll_ = cachedResourcesForCurrentSource();
        commitResourceSourceOrder();
    }

    const assets::CmpPalette* previewPalette()
    {
        if (previewPaletteResolved_)
        {
            return previewPalette_ ? &*previewPalette_ : nullptr;
        }

        previewPaletteResolved_ = true;
        const std::string cmpName = sed::text::trim(options_.previewCmp);
        if (cmpName.empty())
        {
            return nullptr;
        }

        const auto bytes = detail::tryReadPreviewResource(resources_.get(), cmpName, ".cmp");
        if (!bytes)
        {
            return nullptr;
        }

        if (const auto palette = assets::parseCmpPalette(*bytes))
        {
            previewPalette_ = *palette;
            return &*previewPalette_;
        }
        return nullptr;
    }

    std::optional<QImage> loadMatImage(const QString& material)
    {
        const std::string key = material.toStdString();
        if (previewResourceDiskWatcher_)
        {
            previewResourceDiskWatcher_->watchResource(key, ".mat");
            previewResourceDiskWatcher_->watchResource(options_.previewCmp, ".cmp");
            const auto sourceKind = currentSourceKind();
            if (sourceKind == SourceKind::diskRoot || sourceKind == SourceKind::all)
            {
                const auto roots = collectDiskRoots(*resources_, options_.preferredDiskRoot, spec_.includeConfiguredDiskRoots);
                const int selectedRoot = currentSourceIndex();
                for (std::size_t index = 0; index < roots.size(); ++index)
                {
                    if (sourceKind == SourceKind::diskRoot
                        && (selectedRoot < 0 || index != static_cast<std::size_t>(selectedRoot))) continue;
                    // Explicit Source rows and a preferred fallback root may
                    // differ from the locator's ordinary winner. Watch their
                    // real files as well as the effective MAT/CMP dependency.
                    for (const auto& path : candidateDiskPaths(roots[index], spec_, key))
                        previewResourceDiskWatcher_->watchResource(path.string());
                }
            }
        }
        const auto bytes = tryReadBinaryFromSource(
            *resources_,
            spec_,
            currentSourceKind(),
            currentSourceIndex(),
            options_.preferredDiskRoot,
            key);
        if (!bytes)
        {
            return std::nullopt;
        }

        // The selected source may override a same-named MAT from another root
        // or archive. Key the shared decoded-image cache by the actual bytes,
        // palette revision, and resource name so those overrides stay distinct.
        std::uint64_t contentHash = 1469598103934665603ull;
        for (const std::byte value : *bytes)
        {
            contentHash ^= static_cast<std::uint8_t>(value);
            contentHash *= 1099511628211ull;
        }
        const std::string cacheKey =
            std::string("picker-mat|")
            + sed::text::upperAscii(key)
            + "|bytes="
            + std::to_string(bytes->size())
            + ':'
            + std::to_string(contentHash)
            + "|cmp="
            + sed::text::upperAscii(options_.previewCmp)
            + '|'
            + detail::previewResourceRevisionToken(resources_.get(), options_.previewCmp, ".cmp");
        if (const auto cached = detail::findSharedPreviewMaterialImage(cacheKey))
        {
            return cached;
        }

        try
        {
            QImage image = matImageFromBytes(*bytes, previewPalette());
            if (!image.isNull())
            {
                detail::storeSharedPreviewMaterialImage(cacheKey, image);
                return image;
            }
        }
        catch (...)
        {
        }
        return std::nullopt;
    }

    QPixmap matThumbPixmap(const QString& name, const int iconSize)
    {
        if (const QPixmap cached = cachedMatThumbPixmap(name, iconSize); !cached.isNull())
        {
            return cached;
        }

        const std::string cacheKey =
            sed::text::upperAscii(name.toStdString())
            + "|r="
            + detail::previewResourceRevisionToken(resources_.get(), name.toStdString(), ".mat")
            + "|cmp="
            + sed::text::upperAscii(options_.previewCmp)
            + "|cr="
            + detail::previewResourceRevisionToken(resources_.get(), options_.previewCmp, ".cmp")
            + "|"
            + std::to_string(iconSize);
        QPixmap pix{};
        if (const auto image = loadMatImage(name))
        {
            pix = makeMatThumbnail(*image, kMatThumbCacheResolution)
                .scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        matThumbCache_[cacheKey] = pix;
        return pix;
    }

    void updateListModeGridForViewport()
    {
        if (!resourcesList_
            || resourcesList_->viewMode() != QListView::ListMode
            || listModePreferredColumnWidth_ <= 0
            || listModeRowHeight_ <= 0)
        {
            return;
        }

        const QWidget* viewport = resourcesList_->viewport();
        const int availableWidth = viewport ? viewport->width() : resourcesList_->width();
        if (availableWidth <= 0)
        {
            return;
        }

        // Fit an integral number of useful columns to the viewport. Rounding
        // the column width upward keeps the next column outside the viewport;
        // with at most eight visible columns, any final-cell clipping remains
        // inside the delegate's right content padding rather than cutting text.
        const int visibleColumns = std::clamp(
            availableWidth / listModePreferredColumnWidth_,
            1,
            8);
        const int fittedColumnWidth = availableWidth < listModePreferredColumnWidth_
            ? availableWidth
            : (availableWidth + visibleColumns - 1) / visibleColumns;
        const QSize fittedGrid(
            std::max(1, fittedColumnWidth),
            listModeRowHeight_);
        if (resourcesList_->gridSize() == fittedGrid)
        {
            return;
        }

        resourcesList_->setGridSize(fittedGrid);
        for (int row = 0; row < resourcesList_->count(); ++row)
        {
            if (QListWidgetItem* item = resourcesList_->item(row))
            {
                // QListView lays out by gridSize() but paints using the item's
                // styled size hint. Keeping those heights identical prevents
                // the final wrapped row from extending below the viewport.
                item->setSizeHint(fittedGrid);
            }
        }
        resourcesList_->doItemsLayout();
    }

    void rebuildResourceList(
        const bool sourceResourcesChanged = true,
        const int thumbnailStartDelayMs = 0)
    {
        if (!resourcesList_)
        {
            return;
        }

        clearMatThumbPending();
        clearTemplatePreviewPending();
        ++resourceListGeneration_;
        const QString previous = currentResourceName();
        const QSignalBlocker listSignals(resourcesList_);
        const bool listUpdatesWereEnabled = resourcesList_->updatesEnabled();
        resourcesList_->setUpdatesEnabled(false);
        resourcesList_->clear();

        const QString mask = filterEdit_ ? filterEdit_->text().trimmed() : QString{};
        const bool hasMask = !mask.isEmpty();

        const bool useMatThumbs = useMatThumbnails();
        const TemplateListModelPreviewMode modelThumbMode =
            loadTemplateListModelPreviewMode();
        const bool useModelThumbs =
            options_.kind == ResourcePickerKind::model3do
            && modelThumbMode != TemplateListModelPreviewMode::none;
        const bool usesListMode =
            options_.kind != ResourcePickerKind::template_
            && !useModelThumbs
            && !useMatThumbs;
        if (!usesListMode)
        {
            listModePreferredColumnWidth_ = 0;
            listModeRowHeight_ = 0;
            resourcesList_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
            resourcesList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
            resourcesList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        }
        if (options_.kind == ResourcePickerKind::template_)
        {
            resourcesList_->setViewMode(QListView::IconMode);
            resourcesList_->setFlow(QListView::LeftToRight);
            resourcesList_->setWrapping(true);
            resourcesList_->setWordWrap(true);
            resourcesList_->setResizeMode(QListView::Adjust);
            resourcesList_->setUniformItemSizes(true);
            resourcesList_->setIconSize(QSize(96, 96));
            resourcesList_->setGridSize(QSize(136, 166));
        }
        else if (useModelThumbs)
        {
            resourcesList_->setViewMode(QListView::IconMode);
            resourcesList_->setFlow(QListView::LeftToRight);
            resourcesList_->setWrapping(true);
            resourcesList_->setWordWrap(true);
            resourcesList_->setResizeMode(QListView::Adjust);
            resourcesList_->setUniformItemSizes(true);
            resourcesList_->setIconSize(QSize(96, 96));
            resourcesList_->setGridSize(QSize(136, 166));
        }
        else if (useMatThumbs)
        {
            const int iconPx = thumbIconSize();
            const QFontMetrics fm(resourcesList_->font());
            resourcesList_->setViewMode(QListView::IconMode);
            resourcesList_->setFlow(QListView::LeftToRight);
            resourcesList_->setWrapping(true);
            resourcesList_->setWordWrap(true);
            resourcesList_->setResizeMode(QListView::Adjust);
            resourcesList_->setUniformItemSizes(false);
            resourcesList_->setIconSize(QSize(iconPx, iconPx));
            const int textHeight = std::max(fm.height() * 2 + 12, 40);
            const int gridH = iconPx + textHeight;
            const int gridW = std::max(iconPx + std::max(30, iconPx / 2), iconPx + 24);
            resourcesList_->setGridSize(QSize(gridW, gridH));
        }
        else
        {
            resourcesList_->setViewMode(QListView::ListMode);
            resourcesList_->setFlow(QListView::TopToBottom);
            resourcesList_->setWrapping(true);
            resourcesList_->setWordWrap(false);
            resourcesList_->setUniformItemSizes(true);
            resourcesList_->setHorizontalScrollMode(QAbstractItemView::ScrollPerItem);
            resourcesList_->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
            resourcesList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

            QFontMetrics fm(resourcesList_->font());
            int maxTextW = 0;
            const int rowH = std::max(16, fm.height() + 4);
            for (const QString& s : resourcesAll_)
            {
                if (hasMask && !s.contains(mask, Qt::CaseInsensitive))
                {
                    continue;
                }
                maxTextW = std::max(maxTextW, fm.horizontalAdvance(s));
            }
            const int colW = std::clamp(maxTextW + 22, 170, 420);
            listModePreferredColumnWidth_ = colW;
            listModeRowHeight_ = rowH;
            resourcesList_->setGridSize(QSize(colW, rowH));
            updateListModeGridForViewport();
        }

        std::vector<int> pendingThumbRows{};
        pendingThumbRows.reserve(resourcesAll_.size());
        std::vector<int> pendingTemplateRows{};
        pendingTemplateRows.reserve(resourcesAll_.size());
        const QIcon loadingPreviewIcon =
            (options_.kind == ResourcePickerKind::template_ || useModelThumbs)
            ? matPlaceholderIcon(resourcesList_->iconSize().width())
            : QIcon{};

        for (const QString& resourceName : resourcesAll_)
        {
            if (hasMask && !resourceName.contains(mask, Qt::CaseInsensitive))
            {
                continue;
            }

            if (options_.kind == ResourcePickerKind::template_)
            {
                const ThingTemplate* tpl = templateDatabase_.find(resourceName.toStdString());
                const bool baseTemplateBlocked = !options_.allowBaseTemplates && isBaseTemplateName(resourceName);
                if (baseTemplateBlocked)
                {
                    continue;
                }
                const QString parent = tpl ? QString::fromStdString(tpl->parent).trimmed() : QString{};
                QString thingType{};
                if (tpl)
                {
                    if (const TplValue* typeValue = templateDatabase_.findField(tpl->name, "type"))
                    {
                        thingType = QString::fromStdString(typeValue->asString()).trimmed();
                    }
                    else
                    {
                        for (const TplValue& value : tpl->values)
                        {
                            if (QString::fromStdString(value.name).compare(QStringLiteral("type"), Qt::CaseInsensitive) == 0)
                            {
                                thingType = QString::fromStdString(value.asString()).trimmed();
                                break;
                            }
                        }
                    }
                }
                const QString displayName = thingType.isEmpty()
                    ? resourceName
                    : QStringLiteral("%1\n[%2]").arg(resourceName, thingType);
                auto* item = new QListWidgetItem(displayName, resourcesList_);
                item->setData(kActualNameRole, resourceName);
                item->setTextAlignment(Qt::AlignHCenter);
                item->setSizeHint(QSize(132, 158));
                if (tpl)
                {
                    QString tooltip = parent.isEmpty() || parent.compare("none", Qt::CaseInsensitive) == 0
                        ? QString::fromStdString(tpl->desc)
                        : QString("%1\nParent: %2").arg(QString::fromStdString(tpl->desc), parent);
                    if (!thingType.isEmpty())
                    {
                        tooltip = tooltip.trimmed().isEmpty()
                            ? QStringLiteral("Thing Type: %1").arg(thingType)
                            : tooltip + QStringLiteral("\nThing Type: %1").arg(thingType);
                    }
                    item->setToolTip(tooltip.trimmed());
                }
                item->setIcon(loadingPreviewIcon);
                item->setData(kPreviewThumbStateRole, static_cast<int>(PreviewThumbState::loading));
                // Defer preview-field resolution until the row is actually visible.
                // This keeps large template lists from paying the inherited sprite/model lookup cost up front.
                pendingTemplateRows.push_back(resourcesList_->count() - 1);
                continue;
            }
            else if (options_.kind == ResourcePickerKind::model3do && useModelThumbs)
            {
                auto* item = new QListWidgetItem(resourceName, resourcesList_);
                item->setData(kActualNameRole, resourceName);
                item->setTextAlignment(Qt::AlignHCenter);
                item->setSizeHint(QSize(124, 138));
                QIcon cachedIcon{};
                if (detail::findCachedSharedModelPreviewIcon(
                        templatePreviewIconCache_,
                        resources_.get(),
                        options_.previewCmp,
                        resourceName.toStdString(),
                        resourcesList_->iconSize(),
                        modelThumbMode == TemplateListModelPreviewMode::wireframe,
                        cachedIcon))
                {
                    item->setIcon(cachedIcon);
                    item->setData(kPreviewThumbStateRole, static_cast<int>(PreviewThumbState::ready));
                    // A shared thumbnail can make an off-screen row ready
                    // without entering the visible loader. Register its exact
                    // 3DO/CMP/MAT dependencies here so an external edit still
                    // invalidates that already-painted icon while the dialog
                    // remains open.
                    watchModelPreviewDependencies(resourceName.toStdString());
                }
                else
                {
                    item->setIcon(loadingPreviewIcon);
                    item->setData(kPreviewThumbStateRole, static_cast<int>(PreviewThumbState::loading));
                    pendingTemplateRows.push_back(resourcesList_->count() - 1);
                }
                continue;
            }

            auto* item = new QListWidgetItem(resourceName, resourcesList_);
            if (!useMatThumbs)
            {
                // Keep the delegate rectangle identical to the grid cell. Qt's
                // styled size hint can otherwise be taller than gridSize(), so
                // the last wrapped row extends below the viewport.
                item->setSizeHint(resourcesList_->gridSize());
            }
            if (isResourceOnDisk(resourceName))
            {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
            }

            if (useMatThumbs)
            {
                const int iconPx = thumbIconSize();
                item->setSizeHint(resourcesList_->gridSize());
                const QPixmap icon =
                    (options_.kind == ResourcePickerKind::spr)
                        ? cachedSpriteThumbPixmap(resourceName, iconPx)
                        : cachedMatThumbPixmap(resourceName, iconPx);
                if (!icon.isNull())
                {
                    item->setIcon(QIcon(icon));
                }
                else
                {
                    item->setIcon(matPlaceholderIcon(iconPx));
                    pendingThumbRows.push_back(resourcesList_->count() - 1);
                }
            }
        }

        selectResource(previous);
        if (useMatThumbs)
        {
            thumbPendingFlags_.assign(static_cast<std::size_t>(resourcesList_->count()), std::uint8_t{ 0 });
            for (const int row : pendingThumbRows)
            {
                if (row < 0 || row >= static_cast<int>(thumbPendingFlags_.size()))
                {
                    continue;
                }
                thumbPendingFlags_[static_cast<std::size_t>(row)] = std::uint8_t{ 1 };
                ++thumbPendingCount_;
            }
            scheduleMatThumbLoad(0);
        }
        if (options_.kind == ResourcePickerKind::template_ || options_.kind == ResourcePickerKind::model3do)
        {
            templatePreviewQueue_.reset(resourcesList_->count(), pendingTemplateRows);
            scheduleTemplatePreviewLoad(thumbnailStartDelayMs);
            QTimer::singleShot(
                std::max(48, thumbnailStartDelayMs), this, [this]()
                {
                    if (templatePreviewQueue_.pendingCount() > 0)
                    {
                        scheduleTemplatePreviewLoad(0);
                    }
                });
        }
        const QString current = currentResourceName();
        if (selectedEdit_)
        {
            const QSignalBlocker selectedSignals(selectedEdit_);
            selectedEdit_->setText(current);
        }
        updateOkState();
        updateStorageMetadata(current);
        if (sourceResourcesChanged
            || current.compare(previous, Qt::CaseInsensitive) != 0)
        {
            updatePreview();
        }
        if (sourceResourcesChanged)
        {
            syncModel3doDiskWatches();
        }
        resourcesList_->setUpdatesEnabled(listUpdatesWereEnabled);
        resourcesList_->doItemsLayout();
        if (resourcesList_->viewport())
        {
            resourcesList_->viewport()->update();
        }
    }

    void selectResource(const QString& name)
    {
        if (!resourcesList_)
        {
            return;
        }
        const QString desired = ensureDefaultExtension(name);
        if (desired.isEmpty())
        {
            return;
        }

        for (int i = 0; i < resourcesList_->count(); ++i)
        {
            if (QListWidgetItem* item = resourcesList_->item(i))
            {
                // Check the actual resource name first (templates/3DO items store
                // the real filename in kActualNameRole and use a decorated display
                // string in text()), then fall back to text().
                const QString actual = item->data(kActualNameRole).toString().trimmed();
                const bool match = (!actual.isEmpty() && actual.compare(desired, Qt::CaseInsensitive) == 0)
                                || item->text().compare(desired, Qt::CaseInsensitive) == 0;
                if (match)
                {
                    resourcesList_->setCurrentItem(item);
                    resourcesList_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                    return;
                }
            }
        }
    }

    void updateOkState()
    {
        const QString desired = normalizeChosenName();
        if (okButton_)
        {
            okButton_->setEnabled(!desired.isEmpty());
        }
    }

    void refreshSoundPreviewText()
    {
        if ( closing_ || !textPreview_ || !soundPreviewText_ )
        {
            return;
        }
        const QString text = soundPreviewText_->displayText();
        if ( text == textPreview_->toPlainText() )
        {
            return;
        }
        const QTextCursor oldCursor = textPreview_->textCursor();
        const int scrollPosition = textPreview_->verticalScrollBar()->value();
        const int horizontalPosition = textPreview_->horizontalScrollBar()->value();
        textPreview_->setPlainText(text);
        const int lastPosition = std::max(0, textPreview_->document()->characterCount() - 1);
        QTextCursor cursor = textPreview_->textCursor();
        cursor.setPosition(std::min(oldCursor.anchor(), lastPosition));
        cursor.setPosition(std::min(oldCursor.position(), lastPosition), QTextCursor::KeepAnchor);
        textPreview_->setTextCursor(cursor);
        textPreview_->verticalScrollBar()->setValue(scrollPosition);
        textPreview_->horizontalScrollBar()->setValue(horizontalPosition);
    }

    void updatePreview(const bool playSound = false)
    {
        soundPreviewText_.reset();
        selectedModelPreviewRetryTimer_.stop();
        const QString selected = currentResourceName();
        if (!previewStack_ || !matPreviewLabel_ || !textPreview_)
        {
            return;
        }
        updateStorageMetadata(selected);

        if (selected.isEmpty())
        {
            stopSoundPreview();
            matPreviewLabel_->setPixmap(QPixmap{});
            matPreviewLabel_->setText("No selection");
            textPreview_->setPlainText("No selection");
            if (thingPreview_)
            {
                thingPreview_->clearPreview("No previewable resource selected.");
            }
            previewStack_->setCurrentWidget(textPreview_);
            updatePreviewControls();
            return;
        }

        if (options_.kind == ResourcePickerKind::wav)
        {
            previewStack_->setCurrentWidget(textPreview_);
            const QString selection = QStringLiteral("%1:%2:%3")
                .arg(static_cast<int>(currentSourceKind())).arg(currentSourceIndex()).arg(selected);
            if ( !playSound && selection != soundPlayingSelection_ )
            {
                stopSoundPreview();
            }
            soundPreviewSelection_ = selection;
            auto bytes = tryReadBinaryFromSource(
                *resources_,
                spec_,
                currentSourceKind(),
                currentSourceIndex(),
                options_.preferredDiskRoot,
                selected.toStdString());

            QString info = QStringLiteral("Name: %1\n%2")
                .arg(selected, storageMetadataLabel_->text());
            if (bytes)
            {
                const auto metadata = audio::readWavPreviewMetadata(
                    std::span<const std::byte>(*bytes));
                bool playing = false;
                if ( playSound )
                {
                    playing = audio::AppSoundPlayer::instance().playBytes(
                        audio::SoundPlaybackOwner::resourcePicker, selected.toStdString(), std::move(*bytes),
                        soundPreviewLoop_, soundPreviewVolume());
                    soundPlayingSelection_ = playing ? selection : QString{};
                }
                soundPreviewPlayable_ = metadata.has_value() && (!playSound || playing);
                if (metadata)
                {
                    soundPreviewText_ = SoundPreviewText{
                        info + QStringLiteral("\nDuration: "),
                        metadata->duration,
                        QStringLiteral("\nChannels: %1").arg(QString::fromStdString(
                            audio::describeWavChannels(metadata->channelCount)))};
                    info = soundPreviewText_->displayText();
                }
                if ( !soundPreviewPlayable_ )
                {
                    const QString notice = QStringLiteral("\nThis WAV format could not be previewed");
                    info += notice;
                    if ( soundPreviewText_ )
                    {
                        soundPreviewText_->afterDuration += notice;
                    }
                }
            }
            else
            {
                stopSoundPreview();
                info += QStringLiteral("\nResource data could not be loaded");
                qCWarning(logResources).noquote()
                    << "Sound preview resource load failed:" << selected;
            }
            textPreview_->setPlainText(info);
            updatePreviewControls();
            return;
        }

        if (options_.kind == ResourcePickerKind::material)
        {
            previewStack_->setCurrentWidget(matPreviewLabel_);
            if (const auto image = loadMatImage(selected))
            {
                const int maxW = std::max(1, matPreviewLabel_->width() - 12);
                const int maxH = std::max(1, matPreviewLabel_->height() - 12);
                const QPixmap pix = QPixmap::fromImage(image->scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                matPreviewLabel_->setPixmap(pix);
                matPreviewLabel_->setText({});
            }
            else
            {
                matPreviewLabel_->setPixmap(QPixmap{});
                matPreviewLabel_->setText(QString("Couldn't load %1").arg(selected));
                qCWarning(logResources).noquote()
                    << "MAT preview load failed:" << selected;
            }
            updatePreviewControls();
            return;
        }

        if (spec_.showModelPreview)
        {
            if (!thingPreview_)
            {
                return;
            }
            const auto selectedResources = previewResourcesForResource(selected.toStdString());
            thingPreview_->setResourceSnapshot(selectedResources);
            previewStack_->setCurrentWidget(thingPreview_);
            if (options_.kind == ResourcePickerKind::model3do)
            {
                watchModelPreviewDependencies(selected.toStdString());
                std::optional<ThingBox> preparedBox{};
                if (!detail::findSharedPreviewModelBox(
                        selectedResources.get(), selected.toStdString(), preparedBox))
                {
                    const auto kThumbTuning = detail::sharedModelThumbLoadTuning();
                    if (templatePreviewActiveJobs_ < kThumbTuning.maxPrepJobs)
                    {
                        queueTemplateModelPrep(selected.toStdString());
                    }
                    thingPreview_->clearPreview(QString("Loading preview %1...").arg(selected));
                    // QFutureWatcher completion is normally sufficient, but
                    // worker completion or queued event delivery can be
                    // delayed. Poll the shared revision-keyed cache at a low
                    // rate so a completed selected model cannot remain stuck
                    // on the loading page.
                    selectedModelPreviewRetryTimer_.start();
                    updatePreviewControls();
                    return;
                }
            }
            const std::optional<std::vector<std::byte>> bytes =
                (options_.kind == ResourcePickerKind::model3do || options_.kind == ResourcePickerKind::spr)
                    ? std::optional<std::vector<std::byte>>{}
                    : tryReadBinaryFromSource(
                        *resources_,
                        spec_,
                        currentSourceKind(),
                        currentSourceIndex(),
                        options_.preferredDiskRoot,
                        selected.toStdString());
            if (options_.kind == ResourcePickerKind::spr)
            {
                watchSpritePreviewDependencies(selected.toStdString());
            }
            thingPreview_->setPreviewResource(options_.kind, selected, bytes);
            if (options_.kind == ResourcePickerKind::model3do)
            {
                watchModelPreviewDependencies(selected.toStdString());
            }
            updatePreviewControls();
            return;
        }

        if (options_.kind == ResourcePickerKind::template_)
        {
            if (!thingPreview_)
            {
                return;
            }
            previewStack_->setCurrentWidget(thingPreview_);
            if (selected.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0)
            {
                thingPreview_->clearPreview(QStringLiteral("No template preview."));
            }
            else if (const ThingTemplate* tpl = templateDatabase_.find(selected.toStdString()))
            {
                if (const std::optional<std::string> spriteName =
                        resolvedTemplatePreviewField(*tpl, &templateDatabase_, tpl->name, "sprite"))
                {
                    watchSpritePreviewDependencies(*spriteName);
                }
                else
                {
                    std::optional<std::string> modelName =
                        resolvedTemplatePreviewField(*tpl, &templateDatabase_, tpl->name, "model3d");
                    if (!modelName)
                    {
                        modelName = resolvedTemplatePreviewField(
                            *tpl, &templateDatabase_, tpl->name, "model3do");
                    }
                    if (modelName)
                    {
                        watchModelPreviewDependencies(*modelName);
                    }
                }
                thingPreview_->setPreviewTemplate(templateDatabase_, *tpl);
                std::optional<std::string> loadedModel =
                    resolvedTemplatePreviewField(*tpl, &templateDatabase_, tpl->name, "model3d");
                if (!loadedModel)
                {
                    loadedModel = resolvedTemplatePreviewField(
                        *tpl, &templateDatabase_, tpl->name, "model3do");
                }
                if (loadedModel)
                {
                    watchModelPreviewDependencies(*loadedModel);
                }
            }
            else
            {
                qCWarning(logResources).noquote()
                    << "Template preview lookup failed:" << selected;
                thingPreview_->clearPreview(QString("Couldn't preview %1.").arg(selected));
            }
            updatePreviewControls();
            return;
        }

        previewStack_->setCurrentWidget(textPreview_);
        QString info = QString("Name: %1").arg(selected);

        const auto bytes = tryReadBinaryFromSource(
            *resources_,
            spec_,
            currentSourceKind(),
            currentSourceIndex(),
            options_.preferredDiskRoot,
            selected.toStdString());
        info += QLatin1Char('\n') + storageMetadataLabel_->text();
        if (bytes)
        {
            info += QString("\nSize: %1 bytes").arg(bytes->size());
        }
        textPreview_->setPlainText(info);
        updatePreviewControls();
    }

    void updateStorageMetadata(const QString& selected)
    {
        if ( !storageMetadataLabel_ )
        {
            return;
        }

        if ( selected.isEmpty() )
        {
            storageMetadataLabel_->clear();
            storageMetadataLabel_->hide();
            return;
        }

        QString metadata;
        if ( options_.kind == ResourcePickerKind::template_ )
        {
            metadata = selected.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("Storage: No template selected")
                : (templateDatabase_.find(selected.toStdString())
                    ? QStringLiteral("Storage: Template database")
                    : QStringLiteral("Storage: Unavailable"));
        }
        else
        {
            metadata = storageMetadataForResource(
                *resources_,
                spec_,
                currentSourceKind(),
                currentSourceIndex(),
                options_.preferredDiskRoot,
                selected.toStdString());
        }

        storageMetadataLabel_->setText(metadata);
        const bool hasPreviewMetadataField = options_.kind == ResourcePickerKind::material
            || options_.kind == ResourcePickerKind::spr
            || options_.kind == ResourcePickerKind::model3do
            || options_.kind == ResourcePickerKind::template_;
        storageMetadataLabel_->setVisible(hasPreviewMetadataField && !metadata.isEmpty());
    }

    void updatePreviewControls()
    {
        const bool interactivePreview = thingPreview_ && thingPreview_->hasInteractiveGlPreview();
        if (previewProjectionCombo_)
        {
            previewProjectionCombo_->setEnabled(interactivePreview);
        }
        if (previewRenderCombo_)
        {
            previewRenderCombo_->setEnabled(interactivePreview);
        }
        if (previewFitButton_)
        {
            previewFitButton_->setEnabled(interactivePreview);
        }
        if (previewReplayButton_)
        {
            previewReplayButton_->setEnabled(soundPreviewPlayable_);
        }
        if (previewLoopButton_)
        {
            previewLoopButton_->setEnabled(soundPreviewPlayable_);
        }
    }

    std::shared_ptr<const sed::assets::ResourceLocator> resources_{};
    std::shared_ptr<const sed::assets::ResourceLocator> thumbnailResources_{};
    std::unordered_map<std::string, std::shared_ptr<const assets::ResourceLocator>> selectedSourcePreviewResources_{};
    ResourcePickerOptions options_{};
    ResourceKindSpec spec_{};

    QTreeWidget* sources_{};
    QToolButton* editCategoriesButton_{};
    CategoryEditState pendingCategoryEditState_{ CategoryEditState::none };
    QString pendingCategoryEditPath_{};
    QString pendingCategoryEditParentPath_{};
    QString pendingCategoryEditOriginalLabel_{};
    QStackedWidget* previewStack_{};
    QLabel* matPreviewLabel_{};
    QLabel* storageMetadataLabel_{};
    ThingPreviewWidget* thingPreview_{};
    struct SoundPreviewText
    {
        QString beforeDuration{};
        std::chrono::microseconds duration{};
        QString afterDuration{};

        QString displayText() const
        {
            return beforeDuration + formatSoundDuration(duration) + afterDuration;
        }
    };
    QPlainTextEdit* textPreview_{};
    std::optional<SoundPreviewText> soundPreviewText_{};
    QComboBox* previewProjectionCombo_{};
    QComboBox* previewRenderCombo_{};
    QToolButton* previewFitButton_{};
    QToolButton* previewReplayButton_{};
    QToolButton* previewLoopButton_{};
    QSlider* previewVolumeSlider_{};
    QLabel* previewVolumeValue_{};
    int previewVolumePercent_{100};
    bool soundPreviewPlayable_{};
    bool soundPreviewLoop_{};
    QString soundPreviewSelection_{};
    QString soundPlayingSelection_{};
    QStackedWidget* resourceStack_{};
    QLabel* resourceLoadingLabel_{};
    QListWidget* resourcesList_{};
    int listModePreferredColumnWidth_{};
    int listModeRowHeight_{};

    QLineEdit* filterEdit_{};
    QTimer* filterRefreshTimer_{};
    QComboBox* sortCombo_{};
    QLineEdit* selectedEdit_{};
    QToolButton* layoutButton_{};
    QMenu* layoutMenu_{};
    QDialogButtonBox* buttons_{};
    QPushButton* okButton_{};

    std::array<QAction*, 4> layoutActions_{};
    MatThumbLayout thumbLayout_{MatThumbLayout::middle};
    QTimer thumbLoadTimer_{};
    std::vector<std::uint8_t> thumbPendingFlags_{};
    int thumbPendingCount_{};
    int thumbScanCursor_{};
    QTimer templatePreviewLoadTimer_{};
    QTimer selectedModelPreviewRetryTimer_{};
    detail::PreviewResourceDiskWatcher* previewResourceDiskWatcher_{};
    bool archiveReloadWarningShown_{};
    bool closing_{};
    std::uint64_t resourceListGeneration_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    int archiveReloadAttemptCountForTest_{};
#endif
    std::shared_ptr<detail::GlViewModelThumbnailRenderer> modelThumbnailRenderer_{};
    detail::PreviewThumbLoadQueue templatePreviewQueue_{};
    detail::PreviewThumbnailPublicationPacer
        templatePreviewPublicationPacer_{};
    int templatePreviewActiveJobs_{};
    bool templatePreviewLoadInProgress_{ false };
    QTimer templatePreviewKeepaliveTimer_{};
    std::unordered_set<std::string> templatePreviewPrepInFlight_{};
    QIcon matPlaceholderIcon_{};
    int matPlaceholderIconSize_{};
    QIcon matUnavailableIcon_{};
    int matUnavailableIconSize_{};

    std::vector<ResourceSource> sourcesData_{};
    std::vector<QString> resourcesAll_{};
    std::vector<QString> resourcesSourceOrder_{};
    std::unordered_map<std::string, std::vector<QString>> sourceResourcesCache_{};
    std::unordered_set<std::string> diskResourceNames_{};
    bool diskResourceNamesResolved_{};
    std::unordered_map<std::string, QPixmap> matThumbCache_{};
    std::unordered_map<std::string, QPixmap> spriteThumbCache_{};
    std::unordered_map<std::string, QIcon> templatePreviewIconCache_{};
    std::string pickedName_{};
    std::optional<assets::CmpPalette> previewPalette_{};
    bool previewPaletteResolved_{};
    bool initialPopulationScheduled_{};
    bool initialPopulationComplete_{};
    assets::Templates templateDatabase_{};
    std::optional<assets::JlhTree> categoryTreeModel_{};
    std::optional<std::filesystem::path> categoryTreePath_{};
};

} // namespace

std::string resourcePickerTitle(const ResourcePickerKind kind)
{
    switch (kind)
    {
        case ResourcePickerKind::cog:       return "COG Script Picker";
        case ResourcePickerKind::material:  return "Material Picker";
        case ResourcePickerKind::model3do:  return "Model Picker";
        case ResourcePickerKind::key:       return "Keyframe Picker";
        case ResourcePickerKind::wav:       return "Sound Picker";
        case ResourcePickerKind::ai:        return "AI Picker";
        case ResourcePickerKind::template_: return "Template Picker";
        case ResourcePickerKind::message:   return "Message Picker";
        case ResourcePickerKind::cmp:       return "Colormap Picker";
        case ResourcePickerKind::snd:       return "Sound Class Picker";
        case ResourcePickerKind::pup:       return "Puppet Picker";
        case ResourcePickerKind::spr:       return "Sprite Picker";
        case ResourcePickerKind::par:       return "Particle Picker";
        case ResourcePickerKind::uni:       return "String Table Picker";
    }

    return "Resource Picker";
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
std::vector<ResourcePickerStorageMetadataTestSnapshot>
resourcePickerStorageMetadataTraceForTest(
    std::shared_ptr<const assets::ResourceLocator> resources,
    const ResourcePickerOptions& options,
    const std::vector<ResourcePickerStorageMetadataTestStep>& steps)
{
    ResourcePickerDialog dialog(nullptr, std::move(resources), options);
    std::vector<ResourcePickerStorageMetadataTestSnapshot> result{};
    result.reserve(steps.size());
    for ( const ResourcePickerStorageMetadataTestStep& step : steps )
    {
        result.push_back(dialog.storageMetadataStepForTest(step));
    }
    return result;
}

QString resourcePickerSourceDescriptionForTest(const assets::ResourceLocator& resources,
    const ResourcePickerOptions& options, const int sourceKind, const int sourceIndex,
    const std::string& name)
{
    QString description;
    (void)tryReadBinaryFromSource(resources, specForKind(options.kind), static_cast<SourceKind>(sourceKind),
        sourceIndex, options.preferredDiskRoot, name, &description);
    return description;
}

std::pair<std::vector<QImage>, std::vector<QImage>> resourcePickerPaletteChangeForTest(
    std::shared_ptr<const assets::ResourceLocator> resources,
    const ResourcePickerOptions& options, const QString& material,
    const std::function<void()>& change, bool& dependenciesWatched)
{
    ResourcePickerDialog dialog(nullptr, std::move(resources), options);
    const auto image = [&]()
    {
        return options.kind == ResourcePickerKind::spr
            ? dialog.spriteAnimationPaletteImagesForTest(material)
            : std::vector<QImage>{dialog.materialImageForTest(material)};
    };
    const auto before = image();
    auto* watcher = dialog.dependencyWatcherForTest();
    watcher->syncNow();
    dependenciesWatched = watcher->dependencyCountForTest() >= 2;
    change();
    watcher->notifyPathChangedForTest();
    watcher->flushPendingChangeForTest();
    return {before, image()};
}

std::vector<std::string> resourcePickerSourceNamesForTest(
    const assets::ResourceLocator& resources, const ResourcePickerOptions& options,
    const int sourceKind, const int sourceIndex)
{
    const auto names = collectResourcesForSource(resources, specForKind(options.kind),
        static_cast<SourceKind>(sourceKind), sourceIndex, options.preferredDiskRoot);
    std::vector<std::string> result;
    for (const auto& name : names) result.push_back(name.toStdString());
    return result;
}

std::shared_ptr<const assets::ResourceLocator> resourcePickerPreviewResourcesForTest(
    std::shared_ptr<const assets::ResourceLocator> resources,
    const ResourcePickerOptions& options, const int sourceKind, const int sourceIndex)
{
    ResourcePickerDialog dialog(nullptr, std::move(resources), options);
    return dialog.previewResourcesForTest(sourceKind, sourceIndex);
}

bool resourcePickerArchiveReloadRetryForTest(
    std::shared_ptr<const assets::ResourceLocator> resources,
    const ResourcePickerOptions& options,
    const std::function<void()>& corruptArchive,
    const std::function<void()>& repairArchive,
    const std::string& oldEntry,
    const std::string& newEntry,
    int& callbackAttempts,
    bool& oldEntryVisibleAfterFailure,
    bool& oldEntryVisibleAfterRetry,
    bool& newEntryVisibleAfterRetry,
    std::string& oldEntryPayloadAfterFailure,
    std::string& oldEntryPayloadAfterRetry,
    std::string& newEntryPayloadAfterRetry,
    bool& reloadFailureObserved)
{
    oldEntryPayloadAfterFailure.clear();
    oldEntryPayloadAfterRetry.clear();
    newEntryPayloadAfterRetry.clear();
    reloadFailureObserved = false;
    ResourcePickerDialog dialog(nullptr, std::move(resources), options);
    auto* watcher = dialog.dependencyWatcherForTest();
    if ( !watcher )
    {
        return false;
    }
    watcher->syncNow();
    corruptArchive();
    watcher->notifyArchivePathChangedForTest();
    watcher->flushPendingChangeForTest();
    const std::optional<std::string> oldAfterFailure =
        dialog.resourceTextForTest(oldEntry);
    oldEntryVisibleAfterFailure = oldAfterFailure.has_value();
    if ( oldAfterFailure )
    {
        oldEntryPayloadAfterFailure = *oldAfterFailure;
    }
    reloadFailureObserved = dialog.archiveReloadWarningShownForTest();
    repairArchive();
    watcher->flushPendingChangeForTest();
    const std::optional<std::string> oldAfterRetry =
        dialog.resourceTextForTest(oldEntry);
    const std::optional<std::string> newAfterRetry =
        dialog.resourceTextForTest(newEntry);
    oldEntryVisibleAfterRetry = oldAfterRetry.has_value();
    newEntryVisibleAfterRetry = newAfterRetry.has_value();
    if ( oldAfterRetry )
    {
        oldEntryPayloadAfterRetry = *oldAfterRetry;
    }
    if ( newAfterRetry )
    {
        newEntryPayloadAfterRetry = *newAfterRetry;
    }
    callbackAttempts = dialog.archiveReloadAttemptCountForTest();
    return true;
}

ResourcePickerNestedArchiveReloadTestResult
resourcePickerNestedArchiveReloadForTest(
    std::shared_ptr<const assets::ResourceLocator> resources,
    const ResourcePickerOptions& options,
    const std::function<void()>& replaceArchive,
    const std::string& oldEntry,
    const std::string& newEntry)
{
    ResourcePickerNestedArchiveReloadTestResult result{};
    int archiveSignalCount = 0;
    ResourcePickerDialog dialog(nullptr, std::move(resources), options);
    dialog.freezeThingPreviewAutoRotationForTest();
    const auto waitForPreviewSettled = [&dialog]()
    {
        QElapsedTimer timeout{};
        timeout.start();
        int consecutiveSettledPasses = 0;
        while ( timeout.elapsed() < 5000 && consecutiveSettledPasses < 2 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 20);
            const bool settled = dialog.thingPreviewInteractiveForTest()
                && dialog.templatePreviewActiveJobsForTest() == 0
                && detail::previewWorkerPool().activeThreadCount() == 0;
            consecutiveSettledPasses = settled ? consecutiveSettledPasses + 1 : 0;
        }
        return consecutiveSettledPasses == 2;
    };
    dialog.ensurePolished();
    if ( QLayout* pickerLayout = dialog.layout() )
    {
        pickerLayout->activate();
    }
    dialog.show();
    result.previewSettledBefore = waitForPreviewSettled();
    if ( !result.previewSettledBefore )
    {
        return result;
    }

    result.visibleRowsBefore = dialog.visibleResourceNamesForTest();
    result.oldPayloadBefore = dialog.resourceTextForTest(oldEntry);
    result.previewCmpPayloadBefore =
        dialog.resourceTextForTest(options.previewCmp);
    result.previewMaterialPayloadBefore =
        dialog.resourceTextForTest("unused.mat");
    result.previewBefore = dialog.thingPreviewFrameForTest();
    result.previewInteractiveBefore = dialog.thingPreviewInteractiveForTest();
    result.previewTexturesBefore = dialog.thingPreviewTexturesForTest();
    result.previewSolidBefore = dialog.thingPreviewSolidForTest();
    result.previewRenderModeBefore = dialog.thingPreviewRenderModeForTest();
    result.previewSelectedNameBefore = dialog.thingPreviewSelectedNameForTest();
    auto* watcher = dialog.thingPreviewWatcherForTest();
    auto* previewView = dialog.findChild<GlView*>(
        QStringLiteral("resourcePickerThingPreview"));
    if ( !watcher || !previewView )
    {
        return result;
    }

    if ( auto* ownerWatcher = dialog.dependencyWatcherForTest() )
    {
        ownerWatcher->setChangeCallbackForTest({});
        ownerWatcher->setArchiveChangeCallback({});
    }

    const QMetaObject::Connection archiveSignalConnection = QObject::connect(
        previewView,
        &GlView::resourceArchivesChanged,
        &dialog,
        [&archiveSignalCount]()
        {
            ++archiveSignalCount;
        });
    watcher->syncNow();

    replaceArchive();
    watcher->notifyArchivePathChangedForTest();
    watcher->flushPendingChangeForTest();
    result.previewSettledAfter = waitForPreviewSettled();
    for ( int attempt = 0;
          attempt < 8 && watcher->archiveRetryPendingForTest()
              && result.previewSettledAfter;
          ++attempt )
    {
        watcher->flushPendingChangeForTest();
        result.previewSettledAfter = waitForPreviewSettled();
    }
    if ( !result.previewSettledAfter )
    {
        result.archiveSignalCount = archiveSignalCount;
        result.archiveReloadAttemptCount = dialog.archiveReloadAttemptCountForTest();
        result.templatePreviewActiveJobs = dialog.templatePreviewActiveJobsForTest();
        result.previewWorkerActiveThreads =
            detail::previewWorkerPool().activeThreadCount();
        result.archiveRetryPending = watcher->archiveRetryPendingForTest();
        result.ready = true;
        QObject::disconnect(archiveSignalConnection);
        return result;
    }

    result.visibleRowsAfter = dialog.visibleResourceNamesForTest();
    result.oldPayloadAfter = dialog.resourceTextForTest(oldEntry);
    result.newPayloadAfter = dialog.resourceTextForTest(newEntry);
    result.previewCmpPayloadAfter =
        dialog.resourceTextForTest(options.previewCmp);
    result.previewMaterialPayloadAfter =
        dialog.resourceTextForTest("unused.mat");
    result.previewAfter = dialog.thingPreviewFrameForTest();
    result.previewInteractiveAfter = dialog.thingPreviewInteractiveForTest();
    result.previewTexturesAfter = dialog.thingPreviewTexturesForTest();
    result.previewSolidAfter = dialog.thingPreviewSolidForTest();
    result.previewRenderModeAfter = dialog.thingPreviewRenderModeForTest();
    result.previewSelectedNameAfter = dialog.thingPreviewSelectedNameForTest();
    result.archiveSignalCount = archiveSignalCount;
    result.archiveReloadAttemptCount = dialog.archiveReloadAttemptCountForTest();
    result.templatePreviewActiveJobs = dialog.templatePreviewActiveJobsForTest();
    result.previewWorkerActiveThreads =
        detail::previewWorkerPool().activeThreadCount();
    result.archiveRetryPending = watcher->archiveRetryPendingForTest();
    result.ready = true;
    QObject::disconnect(archiveSignalConnection);
    return result;
}
#endif

std::optional<std::string> pickResource(
    QWidget* parent,
    const sed::assets::ResourceLocator& resources,
    const ResourcePickerOptions& options)
{
    return pickResource(
        parent,
        std::make_shared<const sed::assets::ResourceLocator>(resources),
        options);
}

std::optional<std::string> pickResource(
    QWidget* parent,
    std::shared_ptr<const sed::assets::ResourceLocator> resources,
    const ResourcePickerOptions& options)
{
    ResourcePickerDialog dialog(parent, std::move(resources), options);
    dialog.ensurePolished();
    if (QLayout* pickerLayout = dialog.layout())
    {
        pickerLayout->activate();
    }
    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }
    return dialog.result();
}

} // namespace sed::ui
