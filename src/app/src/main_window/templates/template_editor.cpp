#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "app_logging.hpp"
#include "thing/thing_parameter_editor_utils.hpp"
#include "thing/thing_parameter_utils.hpp"
#include "ui/components/category_membership_utils.hpp"
#include "ui/components/dialog_window_policy.hpp"
#include "ui/components/flow_layout.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/path_display_utils.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"
#include "ui/components/preview_thumbnail_list.hpp"
#include "ui/components/template_model_preview_icon.hpp"
#include "ui/components/thing_preview_render_utils.hpp"
#include "ui/components/thing_light_value_dialog.hpp"
#include "ui/components/typed_numeric_text.hpp"
#include "ui/components/viewport_icons.hpp"
#include "render/overlays/template_collision_bounds_guide_utils.hpp"
#include "render/mesh/thing_render_geometry_utils.hpp"
#include "ui/resources/resource_picker_kind_utils.hpp"
#include "ui/resources/template_resource_utils.hpp"
#include "ui/templates/template_editor_utils.hpp"
#include "ui/templates/template_editor_precision_utils.hpp"
#include "thing/thing_parameter_schema.hpp"
#include "sed/assets/jlh.hpp"

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
#include "main_window/templates/template_editor_test_seam.hpp"
#endif

#include <QElapsedTimer>
#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QKeyEvent>
#include <QListWidget>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>

#include <cstdint>
#include <memory>

namespace sed
{

namespace
{

using ::sed::ui::FlowLayout;

constexpr char kTemplateEditorGeometrySetting[] = "templates/editor_geometry";

QStringList buildTemplateFieldNameOptions(
    const ProjectType projectType,
    const std::vector<ThingTemplate>& templates)
{
    QStringList names{};
    for (const ThingTemplate& tpl : templates)
    {
        for (const TplValue& value : tpl.values)
        {
            const QString name = QString::fromStdString(value.name);
            if (!name.trimmed().isEmpty())
            {
                names.push_back(name);
            }
        }
    }

    return thing::parameter_editor::completionNameOptions(projectType, std::move(names));
}

QStringList buildTemplateThingTypeOptions(const ProjectType projectType)
{
    QStringList values{};
    for (const thing::parameter_schema::EnumChoice& choice :
         thing::parameter_schema::enumChoices(projectType, "type"))
    {
        values.push_back(QString::fromLatin1(
            choice.value.data(), static_cast<qsizetype>(choice.value.size())));
    }
    return values;
}

bool isStructuredLightParameter(const std::string_view name) noexcept
{
    return ::sed::text::iequalsAscii(name, "light")
        || ::sed::text::iequalsAscii(name, "lightintensity");
}

QString templateParameterDescription(
    const ProjectType projectType,
    const std::string_view name)
{
    for (const thing::parameter_schema::ParameterDefinition& definition :
         thing::parameter_schema::parameterDefinitions(projectType, true))
    {
        if (::sed::text::iequalsAscii(definition.name, name))
        {
            return QString::fromLatin1(
                definition.description.data(),
                static_cast<qsizetype>(definition.description.size()));
        }
    }
    return {};
}

std::optional<QString> editStructuredLightParameter(
    QWidget* parent,
    const ProjectType projectType,
    const std::string_view name,
    const QString& currentValue,
    QWidget* anchor = nullptr)
{
    if (!isStructuredLightParameter(name))
    {
        return std::nullopt;
    }

    const bool intensity = ::sed::text::iequalsAscii(name, "lightintensity");
    const ui::ThingLightValueDialog::Mode mode = projectType == ProjectType::IJIM
        ? (intensity
            ? ui::ThingLightValueDialog::Mode::ijimColor
            : ui::ThingLightValueDialog::Mode::ijimLight)
        : ui::ThingLightValueDialog::Mode::scalar;
    ui::ThingLightValueDialog dialog(
        parent,
        intensity ? QStringLiteral("Light intensity") : QStringLiteral("Thing light"),
        mode,
        currentValue);
    ui::applyDialogWindowPolicy(dialog);
    dialog.adjustSize();
    ui::positionDialogNearWidget(dialog, anchor);
    return dialog.exec() == QDialog::Accepted
        ? std::optional<QString>{ dialog.valueText() }
        : std::nullopt;
}

std::optional<QString> templateCategoryFileNameForProject(const ProjectType kind)
{
    switch (kind)
    {
        case ProjectType::JKDF2: return QStringLiteral("tpls.jlh");
        case ProjectType::MOTS:  return QStringLiteral("mtpls.jlh");
        case ProjectType::IJIM:  return QStringLiteral("ijimtpls.jlh");
        default:                 return std::nullopt;
    }
}

std::optional<std::filesystem::path> editableTemplateCategoryPath(const ProjectType kind)
{
    const std::optional<QString> fileName = templateCategoryFileNameForProject(kind);
    if (!fileName)
    {
        return std::nullopt;
    }

    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec)
    {
        return std::nullopt;
    }

    for (std::filesystem::path cur = cwd; !cur.empty(); cur = cur.parent_path())
    {
        const std::filesystem::path dataDir = cur / "data";
        if (std::filesystem::exists(dataDir, ec) && std::filesystem::is_directory(dataDir, ec))
        {
            return dataDir / fileName->toStdString();
        }
        if (cur == cur.root_path())
        {
            break;
        }
    }
    return std::nullopt;
}

std::optional<::sed::assets::JlhTree> loadTemplateCategoryTree(
    const ::sed::assets::ResourceLocator* resources,
    const ProjectType kind)
{
    const std::optional<QString> fileName = templateCategoryFileNameForProject(kind);
    if (!fileName)
    {
        return std::nullopt;
    }

    const auto findDataRoot = []() -> std::optional<std::filesystem::path>
    {
        std::error_code ec;
        const std::filesystem::path cwd = std::filesystem::current_path(ec);
        if (!ec)
        {
            for (std::filesystem::path cur = cwd; !cur.empty(); cur = cur.parent_path())
            {
                const std::filesystem::path dataDir = cur / "data";
                if (std::filesystem::exists(dataDir, ec) && std::filesystem::is_directory(dataDir, ec))
                {
                    return dataDir;
                }
                if (cur == cur.root_path())
                {
                    break;
                }
            }
        }
        return std::nullopt;
    };

    if (const auto dataRoot = findDataRoot())
    {
        const std::filesystem::path dataPath = *dataRoot / fileName->toStdString();
        std::error_code ec;
        if (std::filesystem::exists(dataPath, ec))
        {
            try
            {
                return ::sed::assets::loadFromJLH(dataPath);
            }
            catch (const std::exception&)
            {
            }
        }
    }

    if (resources)
    {
        try
        {
            return ::sed::assets::loadFromJLH(fileName->toStdString(), *resources);
        }
        catch (const std::exception&)
        {
        }
    }

    return std::nullopt;
}

QString encodeTemplateCategoryPath(const std::vector<std::size_t>& path)
{
    QStringList parts{};
    for (const std::size_t index : path)
    {
        parts.push_back(QString::number(static_cast<qulonglong>(index)));
    }
    return parts.join(QLatin1Char('/'));
}

std::vector<std::size_t> decodeTemplateCategoryPath(const QString& encoded)
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

::sed::assets::JlhNode* templateCategoryNodeAt(
    ::sed::assets::JlhTree& tree,
    const std::vector<std::size_t>& path) noexcept
{
    std::vector<::sed::assets::JlhNode>* siblings = &tree.roots;
    ::sed::assets::JlhNode* current = nullptr;
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

const ::sed::assets::JlhNode* templateCategoryNodeAt(
    const ::sed::assets::JlhTree& tree,
    const std::vector<std::size_t>& path) noexcept
{
    const std::vector<::sed::assets::JlhNode>* siblings = &tree.roots;
    const ::sed::assets::JlhNode* current = nullptr;
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

std::vector<::sed::assets::JlhNode>* templateCategorySiblingsAt(
    ::sed::assets::JlhTree& tree,
    const std::vector<std::size_t>& parentPath) noexcept
{
    if (parentPath.empty())
    {
        return &tree.roots;
    }

    if (::sed::assets::JlhNode* parent = templateCategoryNodeAt(tree, parentPath))
    {
        return &parent->children;
    }
    return nullptr;
}

void collectTemplateCategoryListNames(
    const std::vector<::sed::assets::JlhNode>& nodes,
    std::unordered_set<std::string>& out)
{
    for (const ::sed::assets::JlhNode& node : nodes)
    {
        if (node.list)
        {
            out.insert(::sed::text::upperAscii(node.list->name));
        }
        if (!node.children.empty())
        {
            collectTemplateCategoryListNames(node.children, out);
        }
    }
}

std::string makeTemplateCategoryListKey(
    const ::sed::assets::JlhTree& tree,
    const QString& label)
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
    collectTemplateCategoryListNames(tree.roots, used);

    std::string candidate = base;
    int suffix = 2;
    while (used.contains(::sed::text::upperAscii(candidate)))
    {
        candidate = base + "_" + std::to_string(suffix++);
    }
    return candidate;
}

bool templateCategoryListContainsEntry(const ::sed::assets::JllList& list, const QString& value)
{
    return ::sed::ui::detail::categoryMembershipContains(list, value);
}

const ::sed::assets::JllList* assignableTemplateCategoryList(const ::sed::assets::JlhNode& node) noexcept
{
    return ::sed::ui::detail::assignableCategoryMembershipList(node);
}

bool isInternalTemplateGeneralCategoryNode(const ::sed::assets::JlhNode& node) noexcept
{
    return ::sed::ui::detail::isInternalGeneralCategoryMembershipNode(node);
}

std::size_t visibleTemplateCategoryChildCount(const ::sed::assets::JlhNode& node) noexcept
{
    std::size_t count = 0;
    for (const ::sed::assets::JlhNode& child : node.children)
    {
        if (!isInternalTemplateGeneralCategoryNode(child))
        {
            ++count;
        }
    }
    return count;
}

std::optional<std::size_t> findTemplateValueIndex(const ThingTemplate& tpl, const std::string_view fieldName)
{
    // Scalar template fields follow engine parser behavior: the last authored
    // occurrence is effective. Repeatable parameters are edited by occurrence
    // and do not use this lookup to choose a row.
    for (std::size_t i = tpl.values.size(); i > 0; --i)
    {
        const std::size_t index = i - 1;
        if (::sed::text::iequalsAscii(tpl.values[index].name, fieldName))
        {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::string> templateFieldStringValue(const ThingTemplate& tpl, const std::string_view fieldName)
{
    if (const std::optional<std::size_t> index = findTemplateValueIndex(tpl, fieldName))
    {
        const TplValue& value = tpl.values[*index];
        const std::string text = ::sed::text::trim(
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
            const std::string trimmed = ::sed::text::trim(value->asString());
            if (!trimmed.empty() && !::sed::text::iequalsAscii(trimmed, "none"))
            {
                return trimmed;
            }
        }
    }

    if (const std::optional<std::string> direct = templateFieldStringValue(tpl, fieldName))
    {
        const std::string trimmed = ::sed::text::trim(*direct);
        if (!trimmed.empty() && !::sed::text::iequalsAscii(trimmed, "none"))
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

        if (currentTpl->parent.empty() || ::sed::text::iequalsAscii(currentTpl->parent, "none"))
        {
            break;
        }

        current = currentTpl->parent;
    }

    return std::nullopt;
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

enum class TemplateListSortMode : int
{
    nameAscending = 0,
    nameDescending = 1,
    sourceOrder = 2,
};

constexpr int kTemplateSourceIndexRole = Qt::UserRole + 880;

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
    const ui::detail::TemplateThumbnailModeSetting normalized =
        ui::detail::normalizeTemplateThumbnailModeSetting(storedValue);
    if (normalized.migrate)
    {
        settings.setValue(kSettingKey, normalized.mode);
    }
    return static_cast<TemplateListModelPreviewMode>(normalized.mode);
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

std::optional<QImage> loadTemplateSpritePreviewImage(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view spriteName)
{
    return ::sed::ui::detail::loadSharedPreviewSpriteImage(resources, cmpName, spriteName);
}

QPixmap makeTemplatePreviewPixmap(const QImage& source, const QSize& targetSize)
{
    return ::sed::ui::detail::makeTemplatePreviewPixmap(source, targetSize);
}

class TemplateTileSelectionDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto* listView = qobject_cast<const QListView*>(option.widget);
        if (!listView || listView->viewMode() != QListView::IconMode)
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
            painter->setPen(QPen(ui::neutralListSelectionBorderColor(), 1.0));
            painter->setBrush(ui::neutralListSelectionFillColor(true));
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

std::vector<QVector3D> modelPreviewWorldVertices(const assets::Model3do& model)
{
    std::vector<QVector3D> vertices{};
    vertices.reserve(256);

    for (const assets::Model3doMesh& mesh : model.meshes)
    {
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

    return ::sed::ui::detail::makeTemplateModelPreviewIcon(
        model,
        targetSize,
        resources,
        cmpName,
        mode == TemplateListModelPreviewMode::wireframe);
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
                ::sed::ui::detail::loadSharedPreviewSprite(resources, *spriteName))
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
        if (const std::optional<ThingBox> box = ::sed::ui::detail::loadSharedPreviewModelBox(resources, *modelName))
        {
            return *box;
        }
    }

    return resolvedTemplatePreviewBBox(tpl, templates, tplName);
}

std::optional<ThingBox> inferTemplatePreviewBox(const ThingTemplate& tpl, const assets::ResourceLocator* resources)
{
    return inferTemplatePreviewBox(tpl, nullptr, std::string_view{}, resources);
}

QString makeUniqueTemplateName(const QString& base, const std::vector<ThingTemplate>& templates, const int skipIndex = -1)
{
    QString candidate = base.trimmed();
    if (candidate.isEmpty())
    {
        candidate = "new_template";
    }

    auto exists = [&](const QString& name) -> bool
    {
        for (std::size_t i = 0; i < templates.size(); ++i)
        {
            if (skipIndex >= 0 && static_cast<int>(i) == skipIndex)
            {
                continue;
            }

            if (QString::fromStdString(templates[i].name).compare(name, Qt::CaseInsensitive) == 0)
            {
                return true;
            }
        }
        return false;
    };

    if (!exists(candidate))
    {
        return candidate;
    }

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        const QString next = QString("%1_%2").arg(candidate).arg(suffix);
        if (!exists(next))
        {
            return next;
        }
    }

    return candidate;
}

class TemplateFieldDialog final : public QDialog
{
public:
    TemplateFieldDialog(
        const QStringList& knownFieldNames,
        const ProjectType projectType,
        QString effectiveThingType,
        std::function<QStringList()> templateNameProvider,
        std::function<const std::vector<ThingTemplate>*()> templateStateProvider,
        std::function<std::optional<QString>(AdjType, const QString&, const QString&, const std::vector<ThingTemplate>*)> browseResource,
        const std::optional<TplValue>& initialValue = std::nullopt,
        const bool lockParameterName = false,
        QWidget* parent = nullptr)
        : QDialog(parent)
        , projectType_(projectType)
        , effectiveThingType_(std::move(effectiveThingType))
        , templateNameProvider_(std::move(templateNameProvider))
        , templateStateProvider_(std::move(templateStateProvider))
        , browseResource_(std::move(browseResource))
        , initialValue_(initialValue)
    {
        setWindowTitle(initialValue ? "Edit Template Parameter" : "Add Template Parameter");
        setModal(true);
        resize(520, 180);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(10);

        auto* form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        form->setSpacing(8);
        layout->addLayout(form);

        nameBox_ = new QComboBox(this);
        nameBox_->setObjectName(QStringLiteral("templateParameterNameBox"));
        nameBox_->setEditable(true);
        nameBox_->addItems(knownFieldNames);
        nameBox_->setInsertPolicy(QComboBox::NoInsert);
        if (nameBox_->lineEdit())
        {
            nameBox_->lineEdit()->setObjectName(QStringLiteral("templateParameterNameEdit"));
        }
        thing::parameter_editor::installNameCompleter(
            nameBox_, knownFieldNames, QStringLiteral("templateParameterNameCompleter"));
        form->addRow("Parameter", nameBox_);

        auto* valueRow = new QWidget(this);
        auto* valueLayout = new QHBoxLayout(valueRow);
        valueLayout->setContentsMargins(0, 0, 0, 0);
        valueLayout->setSpacing(8);

        valueEdit_ = new QLineEdit(valueRow);
        valueEdit_->setObjectName(QStringLiteral("templateParameterValueEdit"));
        valueLayout->addWidget(valueEdit_, 1);

        browseButton_ = new QPushButton("Browse...", valueRow);
        browseButton_->setObjectName(QStringLiteral("templateParameterValuePicker"));
        valueLayout->addWidget(browseButton_);
        form->addRow("Value", valueRow);

        typeLabel_ = new QLabel(this);
        typeLabel_->setStyleSheet("color:#aeb8c8;");
        typeLabel_->setWordWrap(true);
        form->addRow("Type", typeLabel_);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(buttons);

        connect(nameBox_, &QComboBox::editTextChanged, this, [this]()
            {
                updateTypeUi();
            });
        connect(browseButton_, &QPushButton::clicked, this, [this]()
            {
                browseCurrentField();
            });
        connect(buttons, &QDialogButtonBox::accepted, this, &TemplateFieldDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        if (initialValue)
        {
            nameBox_->setEditText(QString::fromStdString(initialValue->name));
            setDisplayedValue(QString::fromStdString(initialValue->asString()));
        }
        nameBox_->setEnabled(!lockParameterName);

        updateTypeUi();
        ui::onUiNumericFormatChanged(this, [this]()
            {
                if (!valueEdit_->isModified())
                {
                    setDisplayedValue(valueEdit_->property("sed.typedNumericCanonicalText").toString());
                }
            });
    }

    std::optional<TplValue> field() const
    {
        const auto canonical = canonicalValueText();
        if (!canonical)
        {
            return std::nullopt;
        }
        return ui::parseTemplateParameterValue(
            nameBox_->currentText(), *canonical, currentSemanticSource());
    }

protected:
    void accept() override
    {
        const QString fieldName = nameBox_->currentText().trimmed();
        if (fieldName.isEmpty())
        {
            QMessageBox::warning(this, "Template Parameter", "Parameter name cannot be empty.");
            return;
        }

        const std::optional<TplValue> value = field();
        if (!value)
        {
            QMessageBox::warning(
                this,
                "Template Parameter",
                QString("The value is invalid for parameter '%1'.").arg(fieldName));
            return;
        }

        QDialog::accept();
    }

private:
    ValueType numericTextType() const
    {
        if (const TplValue* source = currentSemanticSource())
        {
            return ui::typedNumericTextType(*source);
        }
        TplValue value{};
        value.name = nameBox_->currentText().trimmed().toStdString();
        value.type = ::sed::tplValueTypeForName(value.name);
        value.adjType = ::sed::tplAdjTypeForName(value.name);
        return ui::typedNumericTextType(value);
    }

    void setDisplayedValue(const QString& canonical)
    {
        ui::setTypedNumericLineEditText(*valueEdit_, canonical, numericTextType());
        valueEdit_->setProperty("template.numeric.displayBaseline", valueEdit_->text());
    }

    std::optional<QString> canonicalValueText() const
    {
        if (!valueEdit_->isModified()
            && valueEdit_->text() == valueEdit_->property("template.numeric.displayBaseline").toString())
        {
            return valueEdit_->property("sed.typedNumericCanonicalText").toString();
        }
        return ui::canonicalTypedNumericText(valueEdit_->text(), numericTextType(), valueEdit_->locale());
    }

    const TplValue* currentSemanticSource() const
    {
        if (!initialValue_)
        {
            return nullptr;
        }
        const std::string name = nameBox_->currentText().trimmed().toStdString();
        return ::sed::text::iequalsAscii(initialValue_->name, name)
            ? &*initialValue_
            : nullptr;
    }

    void updateTypeUi()
    {
        const QString fieldName = nameBox_->currentText().trimmed();
        const std::string fieldStd = fieldName.toStdString();
        const TplValue* semanticSource = currentSemanticSource();
        const ValueType valueType = semanticSource
            ? semanticSource->type
            : ::sed::tplValueTypeForName(fieldStd);
        const AdjType adjType = semanticSource
            ? semanticSource->adjType
            : ::sed::tplAdjTypeForName(fieldStd);

        const thing::parameter_editor::ValueEditorInfo editorInfo =
            thing::parameter_editor::valueEditorInfo(
                projectType_, fieldStd, effectiveThingType_.toStdString());
        valueEdit_->setPlaceholderText({});

        QString semanticLabel{};
        if (isStructuredLightParameter(fieldStd))
        {
            semanticLabel = projectType_ == ProjectType::IJIM
                ? (::sed::text::iequalsAscii(fieldStd, "light")
                    ? QStringLiteral("vec4 tuple (RGB / range), serialized as (r/g/b/range)")
                    : QStringLiteral("vec3 tuple (RGB), serialized as (r/g/b)"))
                : QStringLiteral("scalar intensity");
        }
        else if (valueType == ValueType::str || valueType == ValueType::unknown)
        {
            if (adjType == AdjType::unk)
            {
                semanticLabel = QStringLiteral("text value (engine-interpreted by name)");
            }
            else
            {
                const std::string_view adjTypeName = ::sed::toString(adjType);
                semanticLabel = QStringLiteral("resource token (%1)").arg(QString::fromLatin1(
                    adjTypeName.data(), static_cast<qsizetype>(adjTypeName.size())));
            }
        }
        else
        {
            semanticLabel = ui::templateParameterTypeLabel(valueType, adjType);
        }
        const QString fieldDescription = templateParameterDescription(projectType_, fieldStd);
        if (!fieldDescription.isEmpty())
        {
            semanticLabel += QStringLiteral(" — ") + fieldDescription;
        }
        typeLabel_->setText(fieldName.isEmpty()
                ? "Choose a parameter name."
                : semanticLabel);
        if (isStructuredLightParameter(fieldStd))
        {
            browseButton_->setText(QStringLiteral("Edit light..."));
            browseButton_->setToolTip(projectType_ == ProjectType::IJIM
                    ? QStringLiteral("Edit the components of this serialized light tuple.")
                    : QStringLiteral("Edit the scalar light intensity."));
            browseButton_->setEnabled(true);
            valueEdit_->setPlaceholderText(projectType_ == ProjectType::IJIM
                    ? (::sed::text::iequalsAscii(fieldStd, "light")
                        ? QStringLiteral("(red/green/blue/range)")
                        : QStringLiteral("(red/green/blue)"))
                    : QStringLiteral("intensity"));
        }
        else if (editorInfo.kind != thing::parameter_editor::ValueEditorKind::none)
        {
            browseButton_->setText(editorInfo.buttonText);
            browseButton_->setToolTip(editorInfo.toolTip);
            browseButton_->setEnabled(true);
        }
        else
        {
            browseButton_->setText(QStringLiteral("Browse..."));
            browseButton_->setToolTip(
                adjType == AdjType::unk
                    ? QStringLiteral("No specialized editor; enter a value directly.")
                    : QStringLiteral("Browse for a resource value."));
            browseButton_->setEnabled(adjType != AdjType::unk);
        }
        if (!valueEdit_->isModified())
        {
            setDisplayedValue(valueEdit_->property("sed.typedNumericCanonicalText").toString());
        }
    }

    void browseCurrentField()
    {
        const QString fieldName = nameBox_->currentText().trimmed();
        if (fieldName.isEmpty())
        {
            return;
        }

        const std::string fieldStd = fieldName.toStdString();
        const QString canonicalText = canonicalValueText().value_or(QString{});
        if (isStructuredLightParameter(fieldStd))
        {
            if (const std::optional<QString> picked = editStructuredLightParameter(
                    this, projectType_, fieldStd, canonicalText, browseButton_))
            {
                setDisplayedValue(*picked);
            }
            return;
        }
        const thing::parameter_editor::ValueEditorInfo editorInfo =
            thing::parameter_editor::valueEditorInfo(
                projectType_, fieldStd, effectiveThingType_.toStdString());
        if (editorInfo.kind != thing::parameter_editor::ValueEditorKind::none)
        {
            if (const std::optional<QString> picked = thing::parameter_editor::openValueEditor(
                    this,
                    projectType_,
                    fieldStd,
                    effectiveThingType_.toStdString(),
                    canonicalText,
                    browseButton_))
            {
                setDisplayedValue(*picked);
            }
            return;
        }

        const TplValue* semanticSource = currentSemanticSource();
        const AdjType adjType = semanticSource
            ? semanticSource->adjType
            : ::sed::tplAdjTypeForName(fieldStd);
        if (adjType == AdjType::unk)
        {
            return;
        }

        if (adjType == AdjType::tpl)
        {
            if (browseResource_)
            {
                const std::optional<QString> picked = browseResource_(
                    adjType,
                    fieldName,
                    canonicalText,
                    templateStateProvider_ ? templateStateProvider_() : nullptr);
                if (picked)
                {
                    setDisplayedValue(*picked);
                }
                return;
            }

            QStringList templateNames = templateNameProvider_ ? templateNameProvider_() : QStringList{};
            if (!templateNames.contains("none", Qt::CaseInsensitive))
            {
                templateNames.push_front("none");
            }

            const qsizetype currentIndex = templateNames.indexOf(canonicalText);
            bool ok = false;
            const QString picked = QInputDialog::getItem(
                this,
                "Choose Template",
                "Template",
                templateNames,
                currentIndex < 0 ? 0 : static_cast<int>(currentIndex),
                false,
                &ok);
            if (ok && !picked.isEmpty())
            {
                setDisplayedValue(picked);
            }
            return;
        }

        if (!browseResource_)
        {
            return;
        }

        const std::optional<QString> picked = browseResource_(
            adjType,
            fieldName,
            canonicalText,
            templateStateProvider_ ? templateStateProvider_() : nullptr);
        if (picked)
        {
            setDisplayedValue(*picked);
        }
    }

    QComboBox* nameBox_{};
    QLineEdit* valueEdit_{};
    QLabel* typeLabel_{};
    QPushButton* browseButton_{};
    ProjectType projectType_{ ProjectType::JKDF2 };
    QString effectiveThingType_{};
    std::function<QStringList()> templateNameProvider_{};
    std::function<const std::vector<ThingTemplate>*()> templateStateProvider_{};
    std::function<std::optional<QString>(AdjType, const QString&, const QString&, const std::vector<ThingTemplate>*)> browseResource_{};
    std::optional<TplValue> initialValue_{};
};

class TemplateEditorDialog final : public QDialog
{
public:
    TemplateEditorDialog(
        std::vector<ThingTemplate> templates,
        const QString& templateFileLabel,
        std::function<std::optional<QString>(AdjType, const QString&, const QString&, const std::vector<ThingTemplate>*)> browseResource,
        const ProjectType projectType,
        std::string previewMasterCmp,
        const assets::ResourceLocator* previewResources,
        const float motsTreeCollisionPreviewMeshRadiusFactor = 0.75f,
        QWidget* parent = nullptr)
        : QDialog(parent)
        , templates_(std::move(templates))
        , browseResource_(std::move(browseResource))
        , projectType_(projectType)
        , previewMasterCmp_(std::move(previewMasterCmp))
        , previewResourcesStorage_(previewResources
              ? std::optional<assets::ResourceLocator>{ *previewResources }
              : std::nullopt)
        , previewResources_(previewResourcesStorage_
              ? &*previewResourcesStorage_
              : nullptr)
        , previewThumbnailResources_(previewResources_
              ? std::make_shared<const assets::ResourceLocator>(*previewResources_)
              : nullptr)
    {
        setWindowTitle("Template Editor");
        setObjectName(QStringLiteral("templateEditorDialog"));
        setMinimumSize(1120, 640);
        const QByteArray savedGeometry = settingsStore().value(
            QString::fromLatin1(kTemplateEditorGeometrySetting)).toByteArray();
        if (savedGeometry.isEmpty() || !restoreGeometry(savedGeometry))
        {
            resize(1460, 800);
        }

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(10);

        auto* subtitle = new QLabel(QString("Project template database: %1").arg(templateFileLabel), this);
        subtitle->setStyleSheet("color:#aeb8c8;");
        root->addWidget(subtitle);

        auto* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setChildrenCollapsible(false);
        root->addWidget(splitter, 1);

        auto* leftPane = new QWidget(splitter);
        leftPane->setMinimumWidth(172);
        leftPane->setMaximumWidth(1400);
        auto* leftLayout = new QVBoxLayout(leftPane);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(8);

        auto* searchRow = new QWidget(leftPane);
        auto* searchRowLayout = new QHBoxLayout(searchRow);
        searchRowLayout->setContentsMargins(0, 0, 0, 0);
        searchRowLayout->setSpacing(6);
        searchEdit_ = new QLineEdit(searchRow);
        searchEdit_->setObjectName(QStringLiteral("templateEditorSearch"));
        searchEdit_->setPlaceholderText("Search templates...");
        searchRowLayout->addWidget(searchEdit_, 1);
        templateSortCombo_ = new QComboBox(searchRow);
        templateSortCombo_->setObjectName(QStringLiteral("templateEditorSort"));
        templateSortCombo_->addItem(QStringLiteral("Name (A-Z)"),
            static_cast<int>(TemplateListSortMode::nameAscending));
        templateSortCombo_->addItem(QStringLiteral("Name (Z-A)"),
            static_cast<int>(TemplateListSortMode::nameDescending));
        templateSortCombo_->addItem(QStringLiteral("Source order"),
            static_cast<int>(TemplateListSortMode::sourceOrder));
        templateSortCombo_->setToolTip(QStringLiteral(
            "Sort templates alphabetically or preserve their order in the TPL file."));
        searchRowLayout->addWidget(templateSortCombo_);
        leftLayout->addWidget(searchRow);

        auto* leftContentSplitter = new QSplitter(Qt::Horizontal, leftPane);
        leftContentSplitter->setChildrenCollapsible(false);
        leftLayout->addWidget(leftContentSplitter, 1);

        categoryTree_ = new ::sed::ui::detail::CategoryTreeWidget(leftContentSplitter);
        categoryTree_->setHeaderHidden(true);
        categoryTree_->setRootIsDecorated(true);
        categoryTree_->setUniformRowHeights(true);
        categoryTree_->setSelectionMode(QAbstractItemView::SingleSelection);
        categoryTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
        categoryTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        categoryTree_->setContextMenuPolicy(Qt::CustomContextMenu);
        categoryTree_->setMinimumWidth(180);

        templateList_ = new ::sed::ui::detail::CategoryAssignableListWidget(leftContentSplitter);
        templateList_->setObjectName(QStringLiteral("templateEditorTemplateList"));
        static_cast<::sed::ui::detail::CategoryAssignableListWidget*>(templateList_)->setCategoryMultiSelectMode(false);
        templateList_->setItemDelegate(new TemplateTileSelectionDelegate(templateList_));
        templateList_->setMovement(QListView::Static);
        templateList_->setResizeMode(QListView::Adjust);
        templateList_->setViewMode(QListView::IconMode);
        templateList_->setFlow(QListView::LeftToRight);
        templateList_->setWrapping(true);
        templateList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        templateList_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        templateList_->setWordWrap(true);
        templateList_->setTextElideMode(Qt::ElideRight);
        templateList_->setIconSize(QSize(96, 96));
        templateList_->setGridSize(QSize(136, 166));
        templateList_->setUniformItemSizes(true);
        templateList_->setSpacing(8);
        templateList_->setMinimumWidth(148);
        templateList_->setContextMenuPolicy(Qt::CustomContextMenu);
        templateList_->viewport()->installEventFilter(this);
        static_cast<::sed::ui::detail::CategoryAssignableListWidget*>(templateList_)->setCategoryDragNameProvider(
            [](const QListWidgetItem* item)
            {
                if (!item)
                {
                    return QString{};
                }
                const QString actual = item->data(Qt::UserRole).toString().trimmed();
                return actual.isEmpty() ? item->text().trimmed() : actual;
            });
        const auto noteThumbnailListInteraction = [this](const int)
        {
            templateIconPublicationPacer_.noteInteraction();
            scheduleTemplateIconLoad(
                templateIconPublicationPacer_.delayUntilQuiet());
        };
        connect(templateList_->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            noteThumbnailListInteraction);
        connect(templateList_->horizontalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            noteThumbnailListInteraction);
        leftContentSplitter->setStretchFactor(0, 0);
        leftContentSplitter->setStretchFactor(1, 1);
        leftContentSplitter->setSizes({ 250, 610 });

        auto* leftButtons = new QWidget(leftPane);
        auto* leftButtonsLayout = new QHBoxLayout(leftButtons);
        leftButtonsLayout->setContentsMargins(0, 0, 0, 0);
        leftButtonsLayout->setSpacing(8);
        auto* newButton = new QPushButton("New", leftButtons);
        auto* duplicateButton = new QPushButton("Duplicate", leftButtons);
        auto* deleteButton = new QPushButton("Delete", leftButtons);
        leftButtonsLayout->addWidget(newButton);
        leftButtonsLayout->addWidget(duplicateButton);
        leftButtonsLayout->addWidget(deleteButton);
        leftLayout->addWidget(leftButtons);

        auto* rightPane = new QWidget(splitter);
        rightPane->setMinimumWidth(280);
        auto* rightLayout = new QVBoxLayout(rightPane);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(10);

        auto* rightSplitter = new QSplitter(Qt::Vertical, rightPane);
        rightSplitter->setChildrenCollapsible(false);
        rightLayout->addWidget(rightSplitter, 1);

        auto* previewPanel = new QWidget(rightSplitter);
        auto* previewLayout = new QVBoxLayout(previewPanel);
        previewLayout->setContentsMargins(0, 0, 0, 0);
        previewLayout->setSpacing(8);

        auto* previewHeader = new QWidget(previewPanel);
        auto* previewHeaderLayout = new QHBoxLayout(previewHeader);
        previewHeaderLayout->setContentsMargins(0, 0, 0, 0);
        previewHeaderLayout->setSpacing(8);
        auto* previewLabel = new QLabel("Preview", previewHeader);
        previewLabel->setStyleSheet("font-weight:600;");
        previewHeaderLayout->addWidget(previewLabel);
        previewHeaderLayout->addStretch(1);

        previewProjectionCombo_ = new QComboBox(previewHeader);
        previewProjectionCombo_->addItem("Orthographic", static_cast<int>(app::ViewportCamera::ProjectionType::Orthographic));
        previewProjectionCombo_->addItem("Perspective", static_cast<int>(app::ViewportCamera::ProjectionType::Perspective));
        previewProjectionCombo_->setToolTip("Preview projection");
        previewHeaderLayout->addWidget(previewProjectionCombo_);

        auto* previewFitButton = new QToolButton(previewHeader);
        previewFitButton->setText("Fit");
        previewFitButton->setToolTip("Fit preview");
        previewHeaderLayout->addWidget(previewFitButton);

        previewRenderCombo_ = new QComboBox(previewHeader);
        previewRenderCombo_->setObjectName(QStringLiteral("templateEditorPreviewRenderMode"));
        previewRenderCombo_->addItem("Textured", 1);
        previewRenderCombo_->addItem("Solid", 0);
        previewRenderCombo_->addItem("Wireframe", -1);
        previewRenderCombo_->addItem("Vertices", -2);
        previewRenderCombo_->setToolTip("Preview shading");
        previewHeaderLayout->addWidget(previewRenderCombo_);

        previewCollisionToggleButton_ = new QToolButton(previewHeader);
        previewCollisionToggleButton_->setObjectName(QStringLiteral("templatePreviewCollisionToggle"));
        previewCollisionToggleButton_->setAutoRaise(true);
        previewCollisionToggleButton_->setAccessibleName(QStringLiteral("Collision preview"));
        previewCollisionToggleButton_->setIconSize(QSize(16, 16));
        previewCollisionToggleButton_->setFixedSize(24, 24);
        previewCollisionToggleButton_->setCheckable(true);
        previewCollisionToggleButton_->setChecked(true);
        previewCollisionToggleButton_->setToolTip(QStringLiteral("Hide collision preview"));
        previewCollisionToggleButton_->setIcon(
            ui::makeViewportIcon(ui::ViewportIcon::thingBoxes));
        previewCollisionToggleButton_->hide();
        previewHeaderLayout->addWidget(previewCollisionToggleButton_);
        previewLayout->addWidget(previewHeader);

        previewCollisionLegend_ = new QLabel(previewPanel);
        previewCollisionLegend_->setObjectName(QStringLiteral("templatePreviewCollisionLegend"));
        previewCollisionLegend_->setWordWrap(true);
        previewCollisionLegend_->setTextFormat(Qt::RichText);
        previewCollisionLegend_->setStyleSheet("QLabel { color:#d0d8e4; font-size:11px; }");
        previewCollisionLegend_->hide();
        previewLayout->addWidget(previewCollisionLegend_);

        previewStack_ = new QStackedWidget(previewPanel);
        previewStack_->setObjectName(QStringLiteral("templatePreviewStack"));
        previewStack_->setMinimumHeight(220);
        previewStack_->setMinimumWidth(0);
        previewStack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        previewEmptyLabel_ = new QLabel("No previewable 3DO model or sprite in this template.", previewStack_);
        previewEmptyLabel_->setObjectName(QStringLiteral("templatePreviewUnavailable"));
        previewEmptyLabel_->setAlignment(Qt::AlignCenter);
        previewEmptyLabel_->setMinimumSize(0, 0);
        previewEmptyLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        previewEmptyLabel_->setStyleSheet(
            "QLabel {"
            " color:#9aa7bb;"
            " border:1px solid rgba(120,135,160,0.35);"
            " border-radius:10px;"
            " background:rgba(6,10,18,0.72);"
            " padding:18px;"
            " }");
        previewStack_->addWidget(previewEmptyLabel_);

        previewSpriteLabel_ = new QLabel(previewStack_);
        previewSpriteLabel_->setObjectName(QStringLiteral("templatePreviewSprite"));
        previewSpriteLabel_->setAlignment(Qt::AlignCenter);
        previewSpriteLabel_->setMinimumSize(0, 0);
        previewSpriteLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        previewSpriteLabel_->setStyleSheet(
            "QLabel {"
            " border:1px solid rgba(120,135,160,0.35);"
            " border-radius:10px;"
            " background:rgba(6,10,18,0.72);"
            " padding:10px;"
            " }");
        previewSpriteLabel_->installEventFilter(this);
        previewStack_->addWidget(previewSpriteLabel_);

        previewSpriteAnimBtn_ = new QToolButton(previewSpriteLabel_);
        previewSpriteAnimBtn_->setAutoRaise(true);
        previewSpriteAnimBtn_->setToolTip("Play / Stop animation");
        previewSpriteAnimBtn_->setFixedSize(26, 26);
        previewSpriteAnimBtn_->setStyleSheet(
            "QToolButton {"
            " background:rgba(28,30,36,200);"
            " border:1px solid rgba(160,170,190,0.45);"
            " border-radius:4px;"
            " color:#dce0e8;"
            " font-weight:bold;"
            " font-size:14px;"
            " }"
            "QToolButton:hover { background:rgba(50,54,64,220); }");
        previewSpriteAnimBtn_->hide();
        connect(previewSpriteAnimBtn_, &QToolButton::clicked, this, [this]()
            {
                if (previewSpriteAnimTimer_ && previewSpriteAnimTimer_->isActive())
                {
                    stopPreviewSpriteAnimation();
                }
                else
                {
                    startPreviewSpriteAnimation();
                }
            });

        previewSpriteAnimSpeedSlider_ = new QSlider(Qt::Horizontal, previewSpriteLabel_);
        previewSpriteAnimSpeedSlider_->setRange(1, 20);
        previewSpriteAnimSpeedSlider_->setValue(10);
        previewSpriteAnimSpeedSlider_->setSingleStep(1);
        previewSpriteAnimSpeedSlider_->setPageStep(2);
        previewSpriteAnimSpeedSlider_->setFixedSize(92, 18);
        previewSpriteAnimSpeedSlider_->setStyleSheet(
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
        previewSpriteAnimSpeedSlider_->hide();
        connect(previewSpriteAnimSpeedSlider_, &QSlider::valueChanged, this, [this](const int)
            {
                updatePreviewSpriteAnimationSpeed();
            });
        connect(previewCollisionToggleButton_, &QToolButton::toggled, this, [this](const bool checked)
            {
                previewShowCollision_ = checked;
                refreshPreview(false);
            });

        previewSpriteAnimTimer_ = new QTimer(this);
        updatePreviewSpriteAnimationSpeed();
        connect(previewSpriteAnimTimer_, &QTimer::timeout, this, [this]()
            {
                if (previewSpriteAnimFrames_.empty())
                {
                    previewSpriteAnimTimer_->stop();
                    return;
                }
                previewSpriteAnimIndex_ =
                    (previewSpriteAnimIndex_ + 1) % static_cast<int>(previewSpriteAnimFrames_.size());
                previewSpriteImage_ = previewSpriteAnimFrames_[static_cast<std::size_t>(previewSpriteAnimIndex_)];
                updatePreviewSpritePixmap();
            });

        previewView_ = new GlView(previewStack_);
        previewView_->setMotsTreeCollisionPreviewMeshRadiusFactor(
            motsTreeCollisionPreviewMeshRadiusFactor);
        previewView_->setObjectName("templatePreviewView");
        previewView_->setResourceLocator(previewResources_);
        previewView_->setResourceArchiveReloadCallback(
            [this]()
            {
                return reloadPreviewArchives();
            });
        connect(
            previewView_,
            &GlView::resourceArchivesChanged,
            this,
            [this]()
            {
                // The nested preview owns the archive notification, while
                // the dialog owns the list cache and pending rows. Reloading
                // completes before GlView emits this signal, so the owner can
                // invalidate and republish its thumbnails from the new
                // locator generation.
                invalidatePreviewResourcesFromDisk();
            });
        previewView_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
        previewView_->setShowGrid(false);
        previewView_->setShowWireframe(false);
        previewView_->setShowVertices(false);
        previewView_->setShowSolid(true);
        previewView_->setShowTextures(true);
        previewView_->setShowThings(true);
        previewView_->setShowEditorCubes(false);
        previewView_->setShowPreviewCubes(false);
        previewView_->setShowThingGizmos(false);
        previewView_->setThingViewMode(GlView::ThingViewMode::Shaded);
        previewView_->setLightingMode(GlView::LightingMode::Gouraud);
        previewView_->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
        previewView_->setViewportPickingEnabled(false);
        previewView_->setHoverPickEnabled(false);
        previewView_->setPreviewInteractionEnabled(true);
        previewView_->installEventFilter(this);
        previewStack_->addWidget(previewView_);
        previewLayout->addWidget(previewStack_, 1);

        previewSpinTimer_ = new QTimer(this);
        previewSpinTimer_->setInterval(
            ::sed::ui::detail::kAssetPreviewSpinIntervalMs);
        previewSpinElapsed_.start();
        const AssetPreviewSpinSettings spinSettings = loadAssetPreviewSpinSettings();
        previewAutoRotateEnabled_ = spinSettings.enabled;
        previewAutoRotateSpeedDegPerSec_ = spinSettings.speedDegPerSec;
        connect(previewSpinTimer_, &QTimer::timeout, this, [this]()
            {
                if (!previewView_ || !previewStack_ || previewStack_->currentWidget() != previewView_)
                {
                    previewSpinElapsed_.restart();
                    return;
                }
                if (!isVisible() || !previewView_->isVisible())
                {
                    previewSpinElapsed_.restart();
                    return;
                }
                if (QApplication::mouseButtons().testFlag(Qt::LeftButton) && previewView_->underMouse())
                {
                    previewSpinElapsed_.restart();
                    return;
                }
                if (::sed::ui::detail::sharedThumbnailRenderBusy())
                {
                    return;
                }
                if (!previewAutoRotateEnabled_ || previewAutoRotateSpeedDegPerSec_ <= 0.0f)
                {
                    previewSpinElapsed_.restart();
                    return;
                }
                const std::int64_t elapsedMs = previewSpinElapsed_.isValid()
                    ? previewSpinElapsed_.restart()
                    : previewSpinTimer_->interval();
                const float stepDeg =
                    ::sed::ui::detail::assetPreviewSpinStepDegrees(
                        previewAutoRotateSpeedDegPerSec_, elapsedMs);
                previewView_->orbitRotate(-stepDeg, 0.0f);
            });
        previewSpinTimer_->start();

        previewRefitTimer_ = new QTimer(this);
        previewRefitTimer_->setSingleShot(true);
        previewRefitTimer_->setInterval(90);
        connect(previewRefitTimer_, &QTimer::timeout, this, [this]()
            {
                fitPreviewView(false);
            });

        templateIconLoadTimer_ = new QTimer(this);
        templateIconLoadTimer_->setSingleShot(true);
        connect(templateIconLoadTimer_, &QTimer::timeout, this, [this]()
            {
                processTemplateIconLoadBatch();
            });
        templateIconKeepaliveTimer_ = new QTimer(this);
        templateIconKeepaliveTimer_->setInterval(150);
        connect(templateIconKeepaliveTimer_, &QTimer::timeout, this, [this]()
            {
                const bool repaired = requeueVisibleLoadingTemplateIconRows();
                if ((repaired || templateIconQueue_.pendingCount() > 0)
                    && templateIconLoadSuspendDepth_ == 0
                    && templateIconActiveJobs_ <= 0
                    && templateIconLoadTimer_
                    && !templateIconLoadTimer_->isActive())
                {
                    scheduleTemplateIconLoad(0);
                }
            });
        templateIconKeepaliveTimer_->start();
        if (QScrollBar* scroll = templateList_->verticalScrollBar())
        {
            scroll->setSingleStep(32);
            connect(scroll, &QScrollBar::valueChanged, this, [this](int)
                {
                    scheduleTemplateIconLoad(0);
                });
        }
        if (QScrollBar* scroll = templateList_->horizontalScrollBar())
        {
            scroll->setSingleStep(32);
            connect(scroll, &QScrollBar::valueChanged, this, [this](int)
                {
                    scheduleTemplateIconLoad(0);
                });
        }

        detailPanel_ = new QWidget(rightSplitter);
        auto* detailLayout = new QVBoxLayout(detailPanel_);
        detailLayout->setContentsMargins(0, 0, 0, 0);
        detailLayout->setSpacing(10);

        auto* form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        form->setSpacing(8);
        detailLayout->addLayout(form);

        nameEdit_ = new QLineEdit(detailPanel_);
        form->addRow("Name", nameEdit_);

        auto* parentRow = new QWidget(detailPanel_);
        auto* parentLayout = new QHBoxLayout(parentRow);
        parentLayout->setContentsMargins(0, 0, 0, 0);
        parentLayout->setSpacing(8);
        parentEdit_ = new QLineEdit(parentRow);
        parentEdit_->setObjectName(QStringLiteral("templateParentEdit"));
        parentLayout->addWidget(parentEdit_, 1);
        auto* parentBrowseButton = new QPushButton("Browse...", parentRow);
        parentLayout->addWidget(parentBrowseButton);
        form->addRow("Parent", parentRow);

        descEdit_ = new QLineEdit(detailPanel_);
        form->addRow("Description", descEdit_);

        typeBox_ = new QComboBox(detailPanel_);
        typeBox_->setEditable(true);
        typeBox_->addItems(buildTemplateThingTypeOptions(projectType_));
        form->addRow("Thing Type", typeBox_);

        auto* bboxToggle = new QToolButton(detailPanel_);
        bboxToggle->setObjectName(QStringLiteral("templateEditorBBoxToggle"));
        bboxToggle->setText(QStringLiteral("Editor Bounds (BBOX)"));
        bboxToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        bboxToggle->setArrowType(Qt::RightArrow);
        bboxToggle->setCheckable(true);
        bboxToggle->setChecked(false);
        bboxToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        bboxToggle->setStyleSheet(QStringLiteral("QToolButton { text-align:left; font-weight:600; }"));
        form->addRow(bboxToggle);

        auto* bboxPanel = new QWidget(detailPanel_);
        bboxPanel->setObjectName(QStringLiteral("templateEditorBBoxPanel"));
        auto* bboxPanelLayout = new QVBoxLayout(bboxPanel);
        bboxPanelLayout->setContentsMargins(0, 0, 0, 0);
        bboxPanelLayout->setSpacing(6);
        auto* bboxHelp = new QLabel(
            QStringLiteral("Authored local editor bounds. SED uses these bounds for surface placement and as a fallback when resource geometry is unavailable."),
            bboxPanel);
        bboxHelp->setObjectName(QStringLiteral("templateEditorBBoxHelp"));
        bboxHelp->setWordWrap(true);
        bboxHelp->setStyleSheet(QStringLiteral("color:#9da8b8; font-size:11px;"));
        bboxPanelLayout->addWidget(bboxHelp);

        auto* bboxWidget = new QWidget(bboxPanel);
        bboxWidget->setObjectName(QStringLiteral("templateEditorBBoxFields"));
        auto* bboxLayout = new QGridLayout(bboxWidget);
        bboxLayout->setContentsMargins(0, 0, 0, 0);
        bboxLayout->setHorizontalSpacing(8);
        bboxLayout->setVerticalSpacing(6);
        static const std::array<const char*, 6> kBBoxLabels = { "X1", "Y1", "Z1", "X2", "Y2", "Z2" };
        for (int i = 0; i < 6; ++i)
        {
            auto* label = new QLabel(QString::fromLatin1(kBBoxLabels[static_cast<std::size_t>(i)]), bboxWidget);
            auto* spin = new ui::ScrubFloatEdit(bboxWidget);
            spin->setObjectName(QStringLiteral("templateEditorBBox") + QString::fromLatin1(kBBoxLabels[static_cast<std::size_t>(i)]));
            spin->setDecimals(6);
            spin->setRange(-1'000'000.0, 1'000'000.0);
            spin->setSingleStep(0.1);
            spin->setStepperButtonWidth(12);
            bboxSpins_[static_cast<std::size_t>(i)] = spin;
            bboxLayout->addWidget(label, i / 3, (i % 3) * 2);
            bboxLayout->addWidget(spin, i / 3, (i % 3) * 2 + 1);
        }
        bboxPanelLayout->addWidget(bboxWidget);
        bboxPanel->hide();
        connect(bboxToggle, &QToolButton::toggled, this, [bboxToggle, bboxPanel](const bool expanded)
            {
                bboxPanel->setVisible(expanded);
                bboxToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            });
        bboxWidget->setToolTip(QStringLiteral(
            "Authored editor bounds stored with the template. Model bounds are measured from 3DO vertices when available; this BBOX still controls surface placement and supplies fallback bounds when resource geometry is unavailable."));
        for (ui::ScrubFloatEdit* spin : bboxSpins_)
        {
            spin->setToolTip(bboxWidget->toolTip());
        }
        form->addRow(bboxPanel);

        auto* fieldsHeader = new QWidget(detailPanel_);
        auto* fieldsHeaderLayout = new QHBoxLayout(fieldsHeader);
        fieldsHeaderLayout->setContentsMargins(0, 0, 0, 0);
        fieldsHeaderLayout->setSpacing(8);
        auto* fieldsLabel = new QLabel("Parameters", fieldsHeader);
        fieldsLabel->setObjectName(QStringLiteral("templateParametersTitle"));
        fieldsLabel->setToolTip(QStringLiteral("Template parameters"));
        fieldsLabel->setStyleSheet("font-weight:600;");
        fieldsHeaderLayout->addWidget(fieldsLabel);
        fieldsHeaderLayout->addStretch(1);
        fieldSearchEdit_ = new QLineEdit(fieldsHeader);
        fieldSearchEdit_->setObjectName(QStringLiteral("templateParametersSearch"));
        fieldSearchEdit_->setPlaceholderText(QStringLiteral("Search parameters"));
        fieldSearchEdit_->setClearButtonEnabled(true);
        fieldSearchEdit_->setMaximumWidth(260);
        fieldsHeaderLayout->addWidget(fieldSearchEdit_);
        detailLayout->addWidget(fieldsHeader);

        fieldsTable_ = new QTableWidget(detailPanel_);
        fieldsTable_->setObjectName(QStringLiteral("templateParametersTable"));
        fieldsTable_->setColumnCount(3);
        fieldsTable_->setHorizontalHeaderLabels({ "Parameter", "Source", "Value" });
        fieldsTable_->horizontalHeaderItem(1)->setToolTip(
            QStringLiteral("Shows whether the value is local, overrides a parent, or is inherited."));
        fieldsTable_->horizontalHeaderItem(2)->setToolTip(
            QStringLiteral("Template values are stored as authored text tokens. The game interprets known parameter names such as size according to their engine meaning."));
        fieldsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        fieldsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        fieldsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        fieldsTable_->setAlternatingRowColors(true);
        fieldsTable_->setStyleSheet(
            "QTableWidget {"
            " alternate-background-color: rgba(255,255,255,0.03);"
            "}"
            "QTableWidget::item:selected {"
            " background: #d7dbe0;"
            " color: #14181f;"
            "}"
            "QTableCornerButton::section {"
            " background: #3c4148;"
            "}");
        fieldsTable_->verticalHeader()->setVisible(false);
        fieldsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        fieldsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        fieldsTable_->setColumnWidth(1, 170);
        fieldsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        detailLayout->addWidget(fieldsTable_, 1);

        auto* fieldButtons = new QWidget(detailPanel_);
        auto* fieldButtonsLayout = new QHBoxLayout(fieldButtons);
        fieldButtonsLayout->setContentsMargins(0, 0, 0, 0);
        fieldButtonsLayout->setSpacing(8);
        auto* addFieldButton = new QPushButton("Add Parameter...", fieldButtons);
        addFieldButton->setObjectName(QStringLiteral("templateAddParameterButton"));
        editFieldButton_ = new QPushButton("Edit Parameter...", fieldButtons);
        editFieldButton_->setObjectName(QStringLiteral("templateEditParameterButton"));
        removeFieldButton_ = new QPushButton("Remove Parameter", fieldButtons);
        removeFieldButton_->setObjectName(QStringLiteral("templateRemoveParameterButton"));
        fieldButtonsLayout->addWidget(addFieldButton);
        fieldButtonsLayout->addWidget(editFieldButton_);
        fieldButtonsLayout->addWidget(removeFieldButton_);
        fieldButtonsLayout->addStretch(1);
        detailLayout->addWidget(fieldButtons);

        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({ 860, 600 });
        rightSplitter->setStretchFactor(0, 0);
        rightSplitter->setStretchFactor(1, 1);
        rightSplitter->setSizes({ 260, 600 });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        if (QPushButton* saveButton = buttons->button(QDialogButtonBox::Save))
        {
            saveButton->setAutoDefault(false);
            saveButton->setDefault(false);
        }
        if (QPushButton* cancelButton = buttons->button(QDialogButtonBox::Cancel))
        {
            cancelButton->setAutoDefault(false);
            cancelButton->setDefault(false);
        }
        root->addWidget(buttons);

        connect(templateList_, &QListWidget::currentRowChanged, this, [this](const int row)
            {
                if (loading_)
                {
                    return;
                }
                const QListWidgetItem* item = templateList_->item(row);
                currentIndex_ = item
                    ? item->data(kTemplateSourceIndexRole).toInt()
                    : -1;
                loadCurrentTemplate();
            });
        connect(fieldSearchEdit_, &QLineEdit::textChanged, this, [this](const QString&)
            {
                applyFieldFilter();
            });
        connect(newButton, &QPushButton::clicked, this, [this]()
            {
                createTemplate();
            });
        connect(duplicateButton, &QPushButton::clicked, this, [this]()
            {
                duplicateCurrentTemplate();
            });
        connect(deleteButton, &QPushButton::clicked, this, [this]()
            {
                deleteCurrentTemplate();
            });
        connect(nameEdit_, &QLineEdit::textEdited, this, [this](const QString&)
            {
                syncBasicFieldsFromWidgets();
            });
        connect(parentEdit_, &QLineEdit::textEdited, this, [this](const QString&)
            {
                syncBasicFieldsFromWidgets();
                reloadEffectiveParameterTable();
            });
        connect(parentEdit_, &QLineEdit::editingFinished, this, [this]()
            {
                if (!loading_)
                {
                    loadCurrentTemplate();
                }
            });
        connect(descEdit_, &QLineEdit::textEdited, this, [this](const QString&)
            {
                syncBasicFieldsFromWidgets();
            });
        connect(typeBox_, &QComboBox::editTextChanged, this, [this](const QString&)
            {
                syncBasicFieldsFromWidgets();
            });
        connect(parentBrowseButton, &QPushButton::clicked, this, [this]()
            {
                if (!hasCurrentTemplate())
                {
                    return;
                }

                if (browseResource_)
                {
                    const std::optional<QString> picked = browseResourcePaused(
                        AdjType::tpl,
                        QStringLiteral("Parent"),
                        parentEdit_->text(),
                        &templates_);
                    if (picked)
                    {
                        parentEdit_->setText(*picked);
                        syncBasicFieldsFromWidgets();
                        loadCurrentTemplate();
                    }
                    return;
                }

                QStringList names = templateNameOptions(currentIndex_);
                if (!names.contains("none", Qt::CaseInsensitive))
                {
                    names.push_front("none");
                }

                const qsizetype currentIndex = names.indexOf(parentEdit_->text());
                bool ok = false;
                const QString picked = QInputDialog::getItem(
                    this,
                    "Choose Parent Template",
                    "Parent",
                    names,
                    currentIndex < 0 ? 0 : static_cast<int>(currentIndex),
                    false,
                    &ok);
                if (ok && !picked.isEmpty())
                {
                    parentEdit_->setText(picked);
                    syncBasicFieldsFromWidgets();
                    loadCurrentTemplate();
                }
            });

        for (int i = 0; i < 6; ++i)
        {
            const auto applyValue = [this, i](const double value)
                {
                    if (loading_ || !hasCurrentTemplate()) return;
                    auto& box = templates_[static_cast<std::size_t>(currentIndex_)].bbox;
                    const std::array<double*, 6> coordinates{
                        &box.x1, &box.y1, &box.z1, &box.x2, &box.y2, &box.z2};
                    *coordinates[static_cast<std::size_t>(i)] = value;
                    syncBasicFieldsFromWidgets();
                };
            auto* edit = bboxSpins_[static_cast<std::size_t>(i)];
            connect(edit, &ui::ScrubFloatEdit::valueCommitted, this, applyValue);
            connect(edit, &ui::ScrubFloatEdit::scrubValueChanged, this, applyValue);
        }

        connect(addFieldButton, &QPushButton::clicked, this, [this]()
            {
                editField(-1);
            });
        connect(editFieldButton_, &QPushButton::clicked, this, [this]()
            {
                editField(selectedFieldRow());
            });
        connect(removeFieldButton_, &QPushButton::clicked, this, [this]()
            {
                removeSelectedField();
            });
        connect(fieldsTable_, &QTableWidget::cellDoubleClicked, this, [this](const int row, const int)
            {
                editField(row);
            });
        connect(fieldsTable_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]()
            {
                updateFieldButtonState();
            });
        connect(previewProjectionCombo_, &QComboBox::currentIndexChanged, this, [this](const int)
            {
                if (!previewView_)
                {
                    return;
                }

                const auto type = static_cast<app::ViewportCamera::ProjectionType>(
                    previewProjectionCombo_->currentData().toInt());
                previewView_->setCameraProjectionType(type);
                QTimer::singleShot(0, previewView_, [view = previewView_]()
                    {
                        if (view)
                        {
                            view->update();
                        }
                    });
                fitPreviewView();
            });
        connect(previewFitButton, &QToolButton::clicked, this, [this]()
            {
                fitPreviewView();
            });
        connect(previewRenderCombo_, &QComboBox::currentIndexChanged, this, [this](const int)
            {
                refreshPreview(false);
            });
        connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString&)
            {
                templateIconPublicationPacer_.noteInteraction();
                applyTemplateFilter();
                scheduleTemplateIconLoad(
                    templateIconPublicationPacer_.delayUntilQuiet());
            });
        connect(templateSortCombo_, &QComboBox::currentIndexChanged, this, [this](const int)
            {
                refreshTemplateList(currentIndex_);
            });
        connect(categoryTree_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
            {
                showTemplateCategoryContextMenu(pos);
            });
        static_cast<::sed::ui::detail::CategoryTreeWidget*>(categoryTree_)->setCategoryDropHandler(
            [this](QTreeWidgetItem* item, const QStringList& names, const Qt::DropAction action)
            {
                return handleTemplateCategoryDrop(item, names, action);
            });
        static_cast<::sed::ui::detail::CategoryTreeWidget*>(categoryTree_)->setCategoryDropActionResolver(
            [this](QTreeWidgetItem* item, const QStringList& names, const Qt::KeyboardModifiers modifiers)
            {
                return resolveTemplateCategoryDropAction(item, names, modifiers);
            });
        connect(templateList_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
            {
                showTemplateCategoryMembershipContextMenu(pos);
            });
        connect(categoryTree_->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this](QWidget*, QAbstractItemDelegate::EndEditHint)
            {
                finalizePendingTemplateCategoryEdit();
            });
        connect(categoryTree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*)
            {
                if (templateListSortMode() == TemplateListSortMode::sourceOrder)
                {
                    refreshTemplateList(currentIndex_);
                }
                else
                {
                    applyTemplateFilter();
                }
                scheduleTemplateIconLoad(0);
            });
        connect(buttons, &QDialogButtonBox::accepted, this, &TemplateEditorDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        previewResourceDiskWatcher_ = new ::sed::ui::detail::PreviewResourceDiskWatcher(
            this,
            [this]()
            {
                invalidatePreviewResourcesFromDisk();
            });
        previewResourceDiskWatcher_->setArchiveChangeCallback(
            [this]()
            {
                return reloadPreviewArchives();
            });
        previewResourceDiskWatcher_->setResourceLocator(previewResources_);
        previewResourceDiskWatcher_->watchResource(previewMasterCmp_, ".cmp");

        rebuildTemplateCategoryTree();
        refreshTemplateList(templates_.empty() ? -1 : 0);
        ui::onUiNumericFormatChanged(this, [this]()
            {
                if (collisionLegendPresentation_ && previewCollisionLegend_
                    && !previewCollisionLegend_->isHidden())
                {
                    const auto presentation = *collisionLegendPresentation_;
                    updateCollisionPreviewLegend(presentation.reason, presentation.shape,
                        presentation.showShape, presentation.showIjimDimensions,
                        presentation.showTemplateBBox, presentation.showBroadPhaseTreeGuide);
                }
            });
    }

    ~TemplateEditorDialog() override
    {
        if ( previewView_ )
        {
            previewView_->setResourceArchiveReloadCallback({});
        }
        if ( previewResourceDiskWatcher_ )
        {
            previewResourceDiskWatcher_->setArchiveChangeCallback({});
        }
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    bool previewPaletteAvailableForTest() { return previewPalette() != nullptr; }

    std::optional<std::string> previewResourceTextForTest(
        const std::string& name) const
    {
        return previewResourcesStorage_
            ? previewResourcesStorage_->tryReadText(name)
            : std::nullopt;
    }

    ::sed::ui::detail::PreviewResourceDiskWatcher*
    previewResourceDiskWatcherForTest() const noexcept
    {
        return previewResourceDiskWatcher_;
    }

    int archiveReloadAttemptCountForTest() const noexcept
    {
        return archiveReloadAttemptCountForTest_;
    }

    bool archiveReloadWarningShownForTest() const noexcept
    {
        return archiveReloadWarningShown_;
    }

    std::size_t previewIconCacheSizeForTest() const noexcept
    {
        return previewIconCache_.size();
    }

    int previewResourceInvalidationCountForTest() const noexcept
    {
        return previewResourceInvalidationCountForTest_;
    }

    int templateIconPendingCountForTest() const noexcept
    {
        return templateIconQueue_.pendingCount();
    }

    int templateIconActiveJobsForTest() const noexcept
    {
        return templateIconActiveJobs_;
    }

    QImage templateListIconForTest(const int row) const
    {
        if ( !templateList_ || row < 0 || row >= templateList_->count() )
        {
            return {};
        }
        const QListWidgetItem* item = templateList_->item(row);
        return item
            ? item->icon().pixmap(templateList_->iconSize()).toImage()
            : QImage{};
    }

    std::vector<std::string> templateListRowsForTest() const
    {
        std::vector<std::string> rows{};
        if ( !templateList_ )
        {
            return rows;
        }

        rows.reserve(static_cast<std::size_t>(templateList_->count()));
        for ( int row = 0; row < templateList_->count(); ++row )
        {
            const QListWidgetItem* item = templateList_->item(row);
            if ( !item )
            {
                continue;
            }
            const QString value = item->data(Qt::UserRole).toString().trimmed();
            rows.push_back(value.toStdString());
        }
        return rows;
    }

    int templateListRowStateForTest(const int row) const noexcept
    {
        if ( !templateList_ || row < 0 || row >= templateList_->count() )
        {
            return -1;
        }
        const QListWidgetItem* item = templateList_->item(row);
        return item
            ? item->data(::sed::ui::detail::kPreviewThumbStateRole).toInt()
            : -1;
    }

    ::sed::ui::detail::PreviewResourceDiskWatcher*
    previewViewResourceDiskWatcherForTest() const noexcept
    {
        return previewView_ ? previewView_->resourceDiskWatcherForTest() : nullptr;
    }

#endif

    const std::vector<ThingTemplate>& templates() const noexcept
    {
        return templates_;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event && event->type() == QEvent::KeyPress
            && watched
            && watched->objectName() == QStringLiteral("templateParameterInlineValueEdit"))
        {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            {
                // Keep Return scoped to the inline field. In a dialog, the
                // parent may otherwise activate its Save button even after
                // the inline editor has emitted editingFinished.
                keyEvent->accept();
                const QPointer<QLineEdit> inlineEdit(qobject_cast<QLineEdit*>(watched));
                QTimer::singleShot(0, this, [inlineEdit]()
                    {
                        if (inlineEdit)
                        {
                            QMetaObject::invokeMethod(
                                inlineEdit.data(), "editingFinished", Qt::DirectConnection);
                        }
                    });
                return true;
            }
        }
        if (watched == previewSpriteLabel_ && event && event->type() == QEvent::Resize)
        {
            updatePreviewSpritePixmap();
            repositionPreviewSpriteAnimControls();
        }
        if (watched == previewView_ && event && event->type() == QEvent::Resize)
        {
            if (previewRefitTimer_ && previewStack_ && previewStack_->currentWidget() == previewView_)
            {
                previewRefitTimer_->start();
            }
        }
        if (watched == templateList_->viewport() && event)
        {
            switch (event->type())
            {
                case QEvent::Wheel:
                {
                    templateIconPublicationPacer_.noteInteraction();
                    auto* wheelEvent = static_cast<QWheelEvent*>(event);
                    if ((wheelEvent->modifiers() & Qt::ShiftModifier) != 0)
                    {
                        if (QScrollBar* scroll = templateList_->verticalScrollBar())
                        {
                            const QPoint delta = wheelEvent->angleDelta();
                            const int dominantDelta = (std::abs(delta.y()) >= std::abs(delta.x()))
                                ? delta.y()
                                : delta.x();
                            if (dominantDelta != 0)
                            {
                                const int direction = dominantDelta < 0 ? 1 : -1;
                                const int pageStep =
                                    ::sed::ui::detail::previewThumbnailPageStep(
                                        scroll->pageStep(),
                                        templateList_->gridSize().height());
                                scroll->setValue(scroll->value() + direction * pageStep);
                            }
                        }
                        scheduleTemplateIconLoad(0);
                        wheelEvent->accept();
                        return true;
                    }
                    scheduleTemplateIconLoad(0);
                    break;
                }
                case QEvent::Resize:
                case QEvent::Show:
                case QEvent::LayoutRequest:
                    scheduleTemplateIconLoad(0);
                    break;
                default:
                    break;
            }
        }
        return QDialog::eventFilter(watched, event);
    }

    void done(const int result) override
    {
        const QByteArray geometry = saveGeometry();
        if (!geometry.isEmpty())
        {
            settingsStore().setValue(
                QString::fromLatin1(kTemplateEditorGeometrySetting), geometry);
        }
        QDialog::done(result);
    }

    void accept() override
    {
        // Default-button acceptance can precede focus loss. Commit only text
        // the user actually edited; untouched clipped display values must not
        // overwrite authored bounds outside the control's editing range.
        for (auto* edit : bboxSpins_)
            if (edit && edit->lineEdit()->isModified())
                QMetaObject::invokeMethod(edit->lineEdit(), "editingFinished", Qt::DirectConnection);
        syncBasicFieldsFromWidgets();

        std::unordered_set<std::string> names{};
        for (const ThingTemplate& tpl : templates_)
        {
            const QString name = QString::fromStdString(tpl.name).trimmed();
            if (name.isEmpty())
            {
                QMessageBox::warning(this, "Template Editor", "Template names cannot be empty.");
                return;
            }

            const std::string key = ::sed::text::upperAscii(name.toStdString());
            if (!names.insert(key).second)
            {
                QMessageBox::warning(
                    this,
                    "Template Editor",
                    QString("Duplicate template name: %1").arg(name));
                return;
            }
        }

        QDialog::accept();
    }

private:
    bool reloadPreviewArchives()
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        ++archiveReloadAttemptCountForTest_;
#endif
        if ( !previewResourcesStorage_ )
        {
            return true;
        }
        if ( templateIconActiveJobs_ > 0 )
        {
            return false;
        }
        try
        {
            previewResourcesStorage_->reloadGobs();
            modelThumbnailRenderer_.reset();
            previewThumbnailResources_ =
                std::make_shared<const assets::ResourceLocator>(
                    *previewResourcesStorage_);
            archiveReloadWarningShown_ = false;
            return true;
        }
        catch ( const std::exception& e )
        {
            if ( !archiveReloadWarningShown_ )
            {
                qCWarning(logResources).noquote()
                    << "Mounted template-preview archive could not be reloaded yet:"
                    << QString::fromUtf8(e.what());
                archiveReloadWarningShown_ = true;
            }
            // Leave the watcher event pending so a partially written archive
            // is retried without requiring another file notification. The old
            // resource snapshot remains live until reloadGobs() succeeds.
            return false;
        }
    }

    enum class TemplateCategoryEditState : std::uint8_t
    {
        none = 0,
        creating = 1,
        renaming = 2,
    };

    static constexpr int kTemplateCategoryNamesRole = Qt::UserRole + 901;
    static constexpr int kTemplateCategoryPathRole = Qt::UserRole + 902;
    static constexpr int kTemplateCategoryHasListRole = Qt::UserRole + 903;
    static constexpr int kTemplateCategoryRootRole = Qt::UserRole + 904;

    bool hasCurrentTemplate() const noexcept
    {
        return currentIndex_ >= 0 && static_cast<std::size_t>(currentIndex_) < templates_.size();
    }

    void rebuildTemplateCategoryTree()
    {
        if (!categoryTree_)
        {
            return;
        }

        categoryTree_->clear();
        templateCategoryTree_ = loadTemplateCategoryTree(previewResources_, projectType_);
        if (!templateCategoryTree_ || templateCategoryTree_->roots.empty())
        {
            categoryTree_->setVisible(false);
            return;
        }

        categoryTree_->setVisible(true);

        auto* root = new QTreeWidgetItem(categoryTree_);
        root->setText(0, "All");
        root->setData(0, kTemplateCategoryRootRole, true);
        root->setData(0, kTemplateCategoryPathRole, QString{});
        root->setData(0, kTemplateCategoryHasListRole, false);

        std::function<void(QTreeWidgetItem*, const ::sed::assets::JlhNode&, std::vector<std::size_t>)> addNode =
            [this, &addNode](QTreeWidgetItem* parent, const ::sed::assets::JlhNode& node, std::vector<std::size_t> path)
            {
                auto* item = new QTreeWidgetItem(parent);
                item->setText(0, QString::fromStdString(node.label));
                item->setChildIndicatorPolicy(
                    visibleTemplateCategoryChildCount(node) == 0
                        ? QTreeWidgetItem::DontShowIndicatorWhenChildless
                        : QTreeWidgetItem::ShowIndicator);
                item->setData(0, kTemplateCategoryPathRole, encodeTemplateCategoryPath(path));
                item->setData(0, kTemplateCategoryHasListRole, assignableTemplateCategoryList(node) != nullptr);
                item->setData(0, kTemplateCategoryRootRole, false);
                QStringList names{};
                ui::appendTemplateCategoryEntriesInSourceOrder(names, node);
                if (!names.isEmpty())
                {
                    item->setData(0, kTemplateCategoryNamesRole, names);
                }
                for (std::size_t i = 0; i < node.children.size(); ++i)
                {
                    if (isInternalTemplateGeneralCategoryNode(node.children[i]))
                    {
                        continue;
                    }
                    std::vector<std::size_t> childPath = path;
                    childPath.push_back(i);
                    addNode(item, node.children[i], std::move(childPath));
                }
            };

        for (std::size_t i = 0; i < templateCategoryTree_->roots.size(); ++i)
        {
            addNode(categoryTree_->invisibleRootItem(), templateCategoryTree_->roots[i], std::vector<std::size_t>{ i });
        }

        categoryTree_->expandAll();
        categoryTree_->setCurrentItem(root);
    }

    QStringList currentTemplateCategoryNames() const
    {
        if (!categoryTree_ || !categoryTree_->isVisible() || !categoryTree_->currentItem())
        {
            return {};
        }
        return categoryTree_->currentItem()->data(0, kTemplateCategoryNamesRole).toStringList();
    }

    ::sed::assets::JllList* currentEditableTemplateCategoryList(QString* encodedPath = nullptr)
    {
        if (!categoryTree_ || !templateCategoryTree_ || !categoryTree_->currentItem())
        {
            return nullptr;
        }

        QTreeWidgetItem* item = categoryTree_->currentItem();
        const QString pathText = item->data(0, kTemplateCategoryPathRole).toString();
        if (encodedPath)
        {
            *encodedPath = pathText;
        }

        if (::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, decodeTemplateCategoryPath(pathText)))
        {
            if (node->list)
            {
                auto list = std::const_pointer_cast<::sed::assets::JllList>(node->list);
                return list.get();
            }

            for (::sed::assets::JlhNode& child : node->children)
            {
                if (child.list
                    && QString::fromStdString(child.label).compare(QStringLiteral("General"), Qt::CaseInsensitive) == 0)
                {
                    auto list = std::const_pointer_cast<::sed::assets::JllList>(child.list);
                    return list.get();
                }
            }
        }
        return nullptr;
    }

    QStringList selectedTemplateNames() const
    {
        QStringList names{};
        if (!templateList_)
        {
            return names;
        }

        QList<QListWidgetItem*> items = templateList_->selectedItems();
        if (items.isEmpty())
        {
            if (QListWidgetItem* current = templateList_->currentItem())
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

            const QString actual = item->data(Qt::UserRole).toString().trimmed();
            const QString name = actual.isEmpty() ? item->text().trimmed() : actual;
            if (!name.isEmpty())
            {
                names.push_back(name);
            }
        }

        names.removeDuplicates();
        return names;
    }

    bool mutateTemplateCategoryMembership(const QString& encodedPath, const QStringList& names, const bool remove)
    {
        if (!templateCategoryTree_ || names.isEmpty())
        {
            return false;
        }

        ::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, decodeTemplateCategoryPath(encodedPath));
        if (!node)
        {
            return false;
        }

        return ::sed::ui::detail::mutateCategoryMembership(
            *node,
            names,
            remove,
            [this]()
            {
                return makeTemplateCategoryListKey(*templateCategoryTree_, QStringLiteral("category"));
            });
    }

    bool moveTemplateCategoryMembership(
        const QString& sourcePath,
        const QString& targetPath,
        const QStringList& names)
    {
        if (!templateCategoryTree_ || names.isEmpty())
        {
            return false;
        }

        ::sed::assets::JlhNode* source = templateCategoryNodeAt(
            *templateCategoryTree_,
            decodeTemplateCategoryPath(sourcePath));
        ::sed::assets::JlhNode* target = templateCategoryNodeAt(
            *templateCategoryTree_,
            decodeTemplateCategoryPath(targetPath));
        if (!source || !target)
        {
            return false;
        }

        return ::sed::ui::detail::moveCategoryMembership(
            *source,
            *target,
            names,
            [this]()
            {
                return makeTemplateCategoryListKey(*templateCategoryTree_, QStringLiteral("category"));
            });
    }

    void refreshTemplateCategoryTreeItemData(const QString& encodedPath)
    {
        if (!categoryTree_ || !templateCategoryTree_ || encodedPath.isEmpty())
        {
            return;
        }

        QTreeWidgetItem* item = findTemplateCategoryItemByPath(encodedPath);
        const ::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, decodeTemplateCategoryPath(encodedPath));
        if (!item || !node)
        {
            return;
        }

        item->setData(0, kTemplateCategoryHasListRole, assignableTemplateCategoryList(*node) != nullptr);
        QStringList names{};
        ui::appendTemplateCategoryEntriesInSourceOrder(names, *node);
        item->setData(0, kTemplateCategoryNamesRole, names);
    }

    bool applyTemplateCategoryMembership(const QString& encodedPath, const QStringList& names, const bool remove)
    {
        if (!templateCategoryTree_)
        {
            return false;
        }

        if (::sed::ui::detail::commitCategoryMembershipMutation(
                *templateCategoryTree_,
                [this, &encodedPath, &names, remove]()
                {
                    return mutateTemplateCategoryMembership(encodedPath, names, remove);
                },
                [this]()
                {
                    return saveTemplateCategoryTree();
                }))
        {
            reloadTemplateCategoryTreePreserveSelection(encodedPath);
            return true;
        }
        return false;
    }

    void populateTemplateAssignableCategoryMenu(
        QMenu* parentMenu,
        const QStringList& names,
        std::unordered_map<QAction*, QString>& actionPaths) const
    {
        if (!parentMenu || !templateCategoryTree_ || names.isEmpty())
        {
            return;
        }

        std::function<void(QMenu*, const ::sed::assets::JlhNode&, const std::vector<std::size_t>&)> addNode =
            [&](QMenu* menu, const ::sed::assets::JlhNode& node, const std::vector<std::size_t>& path)
        {
            if (!menu)
            {
                return;
            }

            const QString label = QString::fromStdString(node.label).trimmed();
            if (label.isEmpty() || isInternalTemplateGeneralCategoryNode(node))
            {
                return;
            }

            const QString encodedPath = encodeTemplateCategoryPath(path);
            const bool hasChildren = !node.children.empty();
            if (hasChildren)
            {
                auto* submenu = new ui::shortcuts::KeySequenceDisplayMenu(label, menu);
                QAction* addHereAction = submenu->addAction(QStringLiteral("Add Here"));
                actionPaths.emplace(addHereAction, encodedPath);

                bool enabled = false;
                if (const ::sed::assets::JllList* list = assignableTemplateCategoryList(node))
                {
                    for (const QString& name : names)
                    {
                        if (!templateCategoryListContainsEntry(*list, name))
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
            if (const ::sed::assets::JllList* list = assignableTemplateCategoryList(node))
            {
                for (const QString& name : names)
                {
                    if (!templateCategoryListContainsEntry(*list, name))
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

        for (std::size_t i = 0; i < templateCategoryTree_->roots.size(); ++i)
        {
            addNode(parentMenu, templateCategoryTree_->roots[i], std::vector<std::size_t>{ i });
        }
    }

    bool currentTemplateCategoryMoveSourcePath(QString* encodedPath = nullptr) const
    {
        if (!categoryTree_ || !categoryTree_->currentItem())
        {
            return false;
        }

        QTreeWidgetItem* current = categoryTree_->currentItem();
        if (current->data(0, kTemplateCategoryRootRole).toBool())
        {
            return false;
        }

        const QString path = current->data(0, kTemplateCategoryPathRole).toString();
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

    Qt::DropAction resolveTemplateCategoryDropAction(
        QTreeWidgetItem* item,
        const QStringList& names,
        const Qt::KeyboardModifiers modifiers) const
    {
        QString sourcePath{};
        if ((modifiers & Qt::ControlModifier) != 0)
        {
            (void)currentTemplateCategoryMoveSourcePath(&sourcePath);
        }
        const Qt::DropAction action = ::sed::ui::detail::resolveCategoryMembershipDropAction(
            item && item->data(0, kTemplateCategoryRootRole).toBool(),
            item ? item->data(0, kTemplateCategoryPathRole).toString() : QString{},
            sourcePath,
            names,
            modifiers);
        if (action != Qt::MoveAction || !templateCategoryTree_ || !item)
        {
            return action;
        }

        const ::sed::assets::JlhNode* source = templateCategoryNodeAt(
            *templateCategoryTree_,
            decodeTemplateCategoryPath(sourcePath));
        const ::sed::assets::JlhNode* target = templateCategoryNodeAt(
            *templateCategoryTree_,
            decodeTemplateCategoryPath(item->data(0, kTemplateCategoryPathRole).toString()));
        return source && target
                && ::sed::ui::detail::categoryMembershipNodesShareStorage(*source, *target)
            ? Qt::IgnoreAction
            : action;
    }

    bool handleTemplateCategoryDrop(QTreeWidgetItem* item, const QStringList& names, const Qt::DropAction action)
    {
        if (!item || names.isEmpty())
        {
            return false;
        }

        const bool root = item->data(0, kTemplateCategoryRootRole).toBool();
        const QString encodedPath = item->data(0, kTemplateCategoryPathRole).toString();
        if (root || encodedPath.isEmpty())
        {
            return false;
        }

        if (!templateCategoryTree_)
        {
            return false;
        }

        QString sourcePath{};
        const bool move =
            action == Qt::MoveAction
            && currentTemplateCategoryMoveSourcePath(&sourcePath)
            && sourcePath != encodedPath;
        if (!::sed::ui::detail::commitCategoryMembershipMutation(
                *templateCategoryTree_,
                [this, &encodedPath, &sourcePath, &names, move]()
                {
                    return move
                        ? moveTemplateCategoryMembership(sourcePath, encodedPath, names)
                        : mutateTemplateCategoryMembership(encodedPath, names, false);
                },
                [this]()
                {
                    return saveTemplateCategoryTree();
                }))
        {
            return false;
        }

        refreshTemplateCategoryTreeItemData(encodedPath);
        if (move)
        {
            refreshTemplateCategoryTreeItemData(sourcePath);
            applyTemplateFilter();
            scheduleTemplateIconLoad(0);
        }
        return true;
    }

    void convertTemplateLeafToBranch(::sed::assets::JlhNode& node)
    {
        if (!node.list)
        {
            return;
        }

        ::sed::assets::JlhNode retained{};
        retained.label = QStringLiteral("General").toStdString();
        retained.list = node.list;
        node.list.reset();
        node.children.insert(node.children.begin(), std::move(retained));
    }

    void showTemplateCategoryMembershipContextMenu(const QPoint& pos)
    {
        if (!templateList_)
        {
            return;
        }

        auto* assignableList = static_cast<::sed::ui::detail::CategoryAssignableListWidget*>(templateList_);
        ui::shortcuts::KeySequenceDisplayMenu menu(this);
        QAction* toggleMultiSelectAction = menu.addAction(
            assignableList->categoryMultiSelectMode()
                ? QStringLiteral("Disable Multi Select")
                : QStringLiteral("Enable Multi Select"));

        const QStringList selectedNames = selectedTemplateNames();
        QString categoryPath{};
        ::sed::assets::JllList* list = currentEditableTemplateCategoryList(&categoryPath);
        QAction* addAction = nullptr;
        QAction* removeAction = nullptr;
        if (!selectedNames.isEmpty() && list)
        {
            menu.addSeparator();
            bool anyMissing = false;
            bool anyPresent = false;
            for (const QString& name : selectedNames)
            {
                const bool contains = templateCategoryListContainsEntry(*list, name);
                anyMissing = anyMissing || !contains;
                anyPresent = anyPresent || contains;
            }

            addAction = menu.addAction(selectedNames.size() > 1
                                           ? QStringLiteral("Add Selected Templates To Current Category")
                                           : QStringLiteral("Add To Current Category"));
            removeAction = menu.addAction(selectedNames.size() > 1
                                              ? QStringLiteral("Remove Selected Templates From Current Category")
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
            populateTemplateAssignableCategoryMenu(addMenu, selectedNames, addTargets);
            if (addMenu->isEmpty())
            {
                addMenu->setEnabled(false);
            }
        }

        QAction* chosen = menu.exec(templateList_->viewport()->mapToGlobal(pos));
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
            applyTemplateCategoryMembership(categoryPath, selectedNames, false);
            return;
        }

        if (chosen == removeAction)
        {
            applyTemplateCategoryMembership(categoryPath, selectedNames, true);
            return;
        }

        if (const auto it = addTargets.find(chosen); it != addTargets.end())
        {
            applyTemplateCategoryMembership(it->second, selectedNames, false);
            return;
        }
    }

    QTreeWidgetItem* findTemplateCategoryItemByPath(const QString& encodedPath) const
    {
        if (!categoryTree_)
        {
            return nullptr;
        }

        const QList<QTreeWidgetItem*> matches =
            categoryTree_->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive);
        for (QTreeWidgetItem* item : matches)
        {
            if (item && item->data(0, kTemplateCategoryPathRole).toString() == encodedPath)
            {
                return item;
            }
        }
        return nullptr;
    }

    QTreeWidgetItem* createTemplateCategoryItem(
        QTreeWidgetItem* parent,
        const ::sed::assets::JlhNode& node,
        const std::vector<std::size_t>& path)
    {
        if (!parent)
        {
            return nullptr;
        }

        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, QString::fromStdString(node.label));
        item->setChildIndicatorPolicy(
            visibleTemplateCategoryChildCount(node) == 0
                ? QTreeWidgetItem::DontShowIndicatorWhenChildless
                : QTreeWidgetItem::ShowIndicator);
        item->setData(0, kTemplateCategoryPathRole, encodeTemplateCategoryPath(path));
        item->setData(0, kTemplateCategoryHasListRole, assignableTemplateCategoryList(node) != nullptr);
        item->setData(0, kTemplateCategoryRootRole, false);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        QStringList names{};
        ui::appendTemplateCategoryEntriesInSourceOrder(names, node);
        if (!names.isEmpty())
        {
            item->setData(0, kTemplateCategoryNamesRole, names);
        }
        return item;
    }

    void beginTemplateCategoryItemEdit(
        QTreeWidgetItem* item,
        const QString& encodedPath,
        const QString& parentPath,
        const TemplateCategoryEditState state,
        const QString& originalLabel)
    {
        if (!categoryTree_ || !item)
        {
            return;
        }

        pendingTemplateCategoryEditState_ = state;
        pendingTemplateCategoryEditPath_ = encodedPath;
        pendingTemplateCategoryEditParentPath_ = parentPath;
        pendingTemplateCategoryEditOriginalLabel_ = originalLabel;

        categoryTree_->setCurrentItem(item);
        categoryTree_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        categoryTree_->editItem(item, 0);
    }

    void finalizePendingTemplateCategoryEdit()
    {
        if (pendingTemplateCategoryEditState_ == TemplateCategoryEditState::none)
        {
            return;
        }

        const TemplateCategoryEditState editState = pendingTemplateCategoryEditState_;
        const QString encodedPath = pendingTemplateCategoryEditPath_;
        const QString parentPath = pendingTemplateCategoryEditParentPath_;
        const QString originalLabel = pendingTemplateCategoryEditOriginalLabel_;
        pendingTemplateCategoryEditState_ = TemplateCategoryEditState::none;
        pendingTemplateCategoryEditPath_.clear();
        pendingTemplateCategoryEditParentPath_.clear();
        pendingTemplateCategoryEditOriginalLabel_.clear();

        QTreeWidgetItem* item = findTemplateCategoryItemByPath(encodedPath);
        const QString label = item ? item->text(0).trimmed() : QString{};
        const std::vector<std::size_t> path = decodeTemplateCategoryPath(encodedPath);

        if (editState == TemplateCategoryEditState::creating && label.isEmpty())
        {
            std::vector<std::size_t> siblingsPath = path;
            const std::size_t index = siblingsPath.empty() ? std::size_t{} : siblingsPath.back();
            if (!siblingsPath.empty())
            {
                siblingsPath.pop_back();
            }
            if (auto* siblings = templateCategorySiblingsAt(*templateCategoryTree_, siblingsPath))
            {
                if (index < siblings->size())
                {
                    siblings->erase(siblings->begin() + static_cast<std::ptrdiff_t>(index));
                }
            }
            reloadTemplateCategoryTreePreserveSelection(parentPath);
            return;
        }

        if (editState == TemplateCategoryEditState::renaming && label.isEmpty())
        {
            if (::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, path))
            {
                node->label = originalLabel.toStdString();
            }
            reloadTemplateCategoryTreePreserveSelection(encodedPath);
            return;
        }

        if (::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, path))
        {
            node->label = label.toStdString();
            if (saveTemplateCategoryTree())
            {
                reloadTemplateCategoryTreePreserveSelection(encodedPath);
            }
            else
            {
                reloadTemplateCategoryTreePreserveSelection(parentPath);
            }
        }
    }

    bool saveTemplateCategoryTree()
    {
        const std::optional<std::filesystem::path> path = editableTemplateCategoryPath(projectType_);
        if (!path || !templateCategoryTree_)
        {
            return false;
        }

        try
        {
            ::sed::assets::saveToJLH(*templateCategoryTree_, *path);
            return true;
        }
        catch (const std::exception& ex)
        {
            QMessageBox::warning(
                this,
                QStringLiteral("Save categories failed"),
                QStringLiteral("Couldn't save template categories.\n%1").arg(QString::fromUtf8(ex.what())));
            return false;
        }
    }

    void reloadTemplateCategoryTreePreserveSelection(const QString& selectedPath = QString{})
    {
        const int previousRow = templateList_ ? templateList_->currentRow() : -1;
        rebuildTemplateCategoryTree();
        if (!selectedPath.isEmpty() && categoryTree_)
        {
            const QList<QTreeWidgetItem*> matches = categoryTree_->findItems(
                QStringLiteral("*"),
                Qt::MatchWildcard | Qt::MatchRecursive);
            for (QTreeWidgetItem* item : matches)
            {
                if (item && item->data(0, kTemplateCategoryPathRole).toString() == selectedPath)
                {
                    categoryTree_->setCurrentItem(item);
                    break;
                }
            }
        }
        applyTemplateFilter();
        if (previousRow >= 0 && templateList_ && previousRow < templateList_->count())
        {
            templateList_->setCurrentRow(previousRow);
        }
    }

    void showTemplateCategoryContextMenu(const QPoint& pos)
    {
        if (!categoryTree_ || !templateCategoryTree_)
        {
            return;
        }

        QTreeWidgetItem* item = categoryTree_->itemAt(pos);
        const bool clickedEmpty = (item == nullptr);
        const bool root = !item || item->data(0, kTemplateCategoryRootRole).toBool();
        const QString encodedPath = item ? item->data(0, kTemplateCategoryPathRole).toString() : QString{};
        const std::vector<std::size_t> path = decodeTemplateCategoryPath(encodedPath);
        const ::sed::assets::JlhNode* node = (!root && templateCategoryTree_) ? templateCategoryNodeAt(*templateCategoryTree_, path) : nullptr;
        const bool hasList = node && assignableTemplateCategoryList(*node);

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
        const QStringList selectedNames = selectedTemplateNames();
        if (item && !root && !selectedNames.isEmpty())
        {
            bool anyMissing = false;
            bool anyPresent = false;
            if (const ::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, path))
            {
                const ::sed::assets::JllList* list = assignableTemplateCategoryList(*node);
                if (list)
                {
                    for (const QString& name : selectedNames)
                    {
                        const bool contains = templateCategoryListContainsEntry(*list, name);
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
                                              ? QStringLiteral("Add Selected Templates Here")
                                              : QStringLiteral("Add Selected Template Here"));
            assignAction->setEnabled(anyMissing);
            if (hasList)
            {
                removeAction = menu.addAction(selectedNames.size() > 1
                                                  ? QStringLiteral("Remove Selected Templates From Here")
                                                  : QStringLiteral("Remove Selected Template From Here"));
                removeAction->setEnabled(anyPresent);
            }
        }

        QAction* chosen = menu.exec(categoryTree_->viewport()->mapToGlobal(pos));
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
            if (auto* siblings = templateCategorySiblingsAt(*templateCategoryTree_, parentPath))
            {
                siblings->push_back(::sed::assets::JlhNode{ "", {}, {} });
                const std::vector<std::size_t> newPath = [&]()
                {
                    std::vector<std::size_t> p = parentPath;
                    p.push_back(siblings->size() - 1);
                    return p;
                }();
                QTreeWidgetItem* parentItem = (clickedEmpty || root) ? categoryTree_->invisibleRootItem() : item->parent();
                if (QTreeWidgetItem* created =
                        createTemplateCategoryItem(
                            parentItem ? parentItem : categoryTree_->invisibleRootItem(),
                            siblings->back(),
                            newPath))
                {
                    beginTemplateCategoryItemEdit(
                        created,
                        encodeTemplateCategoryPath(newPath),
                        encodeTemplateCategoryPath(parentPath),
                        TemplateCategoryEditState::creating,
                        QString{});
                }
            }
            return;
        }

        if (chosen == addSubcategoryAction)
        {
            if (::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, path))
            {
                if (node->list)
                {
                    convertTemplateLeafToBranch(*node);
                }
                node->children.push_back(::sed::assets::JlhNode{ "", {}, {} });
                std::vector<std::size_t> newPath = path;
                newPath.push_back(node->children.size() - 1);
                if (QTreeWidgetItem* created = createTemplateCategoryItem(item, node->children.back(), newPath))
                {
                    item->setExpanded(true);
                    beginTemplateCategoryItemEdit(
                        created,
                        encodeTemplateCategoryPath(newPath),
                        encodedPath,
                        TemplateCategoryEditState::creating,
                        QString{});
                }
            }
            return;
        }

        if (chosen == addLeafAction)
        {
            if (root)
            {
                auto list = std::make_shared<::sed::assets::JllList>();
                list->name = makeTemplateCategoryListKey(*templateCategoryTree_, QStringLiteral("category"));
                templateCategoryTree_->roots.push_back(::sed::assets::JlhNode{ "", list, {} });
                const std::vector<std::size_t> newPath{ templateCategoryTree_->roots.size() - 1 };
                if (QTreeWidgetItem* created = createTemplateCategoryItem(categoryTree_->invisibleRootItem(), templateCategoryTree_->roots.back(), newPath))
                {
                    beginTemplateCategoryItemEdit(
                        created,
                        encodeTemplateCategoryPath(newPath),
                        QString{},
                        TemplateCategoryEditState::creating,
                        QString{});
                }
                return;
            }

            if (::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, path))
            {
                auto list = std::make_shared<::sed::assets::JllList>();
                list->name = makeTemplateCategoryListKey(*templateCategoryTree_, QStringLiteral("category"));
                node->children.push_back(::sed::assets::JlhNode{ "", list, {} });
                std::vector<std::size_t> newPath = path;
                newPath.push_back(node->children.size() - 1);
                if (QTreeWidgetItem* created = createTemplateCategoryItem(item, node->children.back(), newPath))
                {
                    item->setExpanded(true);
                    beginTemplateCategoryItemEdit(
                        created,
                        encodeTemplateCategoryPath(newPath),
                        encodedPath,
                        TemplateCategoryEditState::creating,
                        QString{});
                }
            }
            return;
        }

        if (chosen == renameAction)
        {
            ::sed::assets::JlhNode* node = templateCategoryNodeAt(*templateCategoryTree_, path);
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
            beginTemplateCategoryItemEdit(
                item,
                encodedPath,
                encodeTemplateCategoryPath(parentPath),
                TemplateCategoryEditState::renaming,
                QString::fromStdString(node->label));
            return;
        }

        if (chosen == deleteAction)
        {
            const QString categoryName = item->text(0);
            QMessageBox confirmation(
                QMessageBox::Question,
                QStringLiteral("Delete category"),
                QStringLiteral("Delete \"%1\" and its contents?")
                    .arg(categoryName),
                QMessageBox::Yes | QMessageBox::No,
                this);
            confirmation.setProperty(
                kMessageBoxLoggingDestructiveProperty, true);
            confirmation.setProperty(kMessageBoxLoggingContextProperty,
                QStringLiteral("Template category: %1")
                    .arg(categoryName));
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
            if (auto* siblings = templateCategorySiblingsAt(*templateCategoryTree_, parentPath))
            {
                if (index < siblings->size())
                {
                    siblings->erase(siblings->begin() + static_cast<std::ptrdiff_t>(index));
                    if (saveTemplateCategoryTree())
                    {
                        reloadTemplateCategoryTreePreserveSelection(encodeTemplateCategoryPath(parentPath));
                    }
                }
            }
            return;
        }

        if (chosen == assignAction)
        {
            applyTemplateCategoryMembership(encodedPath, selectedNames, false);
            return;
        }

        if (chosen == removeAction)
        {
            applyTemplateCategoryMembership(encodedPath, selectedNames, true);
        }
    }

    QStringList templateNameOptions(const int skipIndex = -1) const
    {
        QStringList names{};
        for (std::size_t i = 0; i < templates_.size(); ++i)
        {
            if (skipIndex >= 0 && static_cast<int>(i) == skipIndex)
            {
                continue;
            }

            const QString name = QString::fromStdString(templates_[i].name).trimmed();
            if (!name.isEmpty())
            {
                names.push_back(name);
            }
        }

        names.removeDuplicates();
        std::sort(names.begin(), names.end(), [](const QString& a, const QString& b)
            {
                return a.compare(b, Qt::CaseInsensitive) < 0;
            });
        return names;
    }

    void suspendTemplateIconLoading()
    {
        ++templateIconLoadSuspendDepth_;
        if (templateIconLoadTimer_)
        {
            templateIconLoadTimer_->stop();
        }
    }

    void resumeTemplateIconLoading()
    {
        if (templateIconLoadSuspendDepth_ > 0)
        {
            --templateIconLoadSuspendDepth_;
        }
        if (templateIconLoadSuspendDepth_ == 0 && templateIconQueue_.pendingCount() > 0)
        {
            scheduleTemplateIconLoad(0);
        }
    }

    std::optional<QString> browseResourcePaused(
        const AdjType adjType,
        const QString& fieldName,
        const QString& currentValue,
        const std::vector<ThingTemplate>* templateOverrides)
    {
        if (!browseResource_)
        {
            return std::nullopt;
        }

        struct TemplateIconLoadPauseScope final
        {
            TemplateEditorDialog* dialog;
            explicit TemplateIconLoadPauseScope(TemplateEditorDialog* owner)
                : dialog(owner)
            {
                dialog->suspendTemplateIconLoading();
            }
            ~TemplateIconLoadPauseScope()
            {
                dialog->resumeTemplateIconLoading();
            }
        } pause(this);

        return browseResource_(adjType, fieldName, currentValue, templateOverrides);
    }

    void clearTemplateIconPending()
    {
        if (templateIconLoadTimer_)
        {
            templateIconLoadTimer_->stop();
        }
        templateIconQueue_.clear();
    }

    void watchTemplateModelPreviewDependencies(const std::string_view modelName)
    {
        if (!previewResourceDiskWatcher_)
        {
            return;
        }

        previewResourceDiskWatcher_->watchResource(modelName, ".3do");
        previewResourceDiskWatcher_->watchResource(previewMasterCmp_, ".cmp");
        std::shared_ptr<const assets::Model3do> model{};
        if (::sed::ui::detail::findCachedSharedPreviewModel(
                previewThumbnailResources_.get(), modelName, model)
            && model)
        {
            for (const std::string& material : model->materials)
            {
                previewResourceDiskWatcher_->watchResource(material, ".mat");
            }
        }
    }

    void watchTemplateSpritePreviewDependencies(const std::string_view spriteName)
    {
        if (!previewResourceDiskWatcher_)
        {
            return;
        }

        previewResourceDiskWatcher_->watchResource(spriteName, ".spr");
        previewResourceDiskWatcher_->watchResource(previewMasterCmp_, ".cmp");
        if (const std::shared_ptr<const assets::SprFile> sprite =
                ::sed::ui::detail::loadSharedPreviewSprite(previewResources_, spriteName))
        {
            previewResourceDiskWatcher_->watchResource(sprite->material, ".mat");
        }
    }

    void invalidatePreviewResourcesFromDisk()
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        ++previewResourceInvalidationCountForTest_;
#endif
        ::sed::ui::detail::clearSharedPreviewAssetCaches();
        previewIconCache_.clear();
        modelThumbnailRenderer_.reset();
        previewPalette_.reset();
        previewPaletteResolved_ = false;
        if (previewView_)
        {
            previewView_->reloadCachedResources();
        }

        clearTemplateIconPending();
        std::vector<int> pendingRows{};
        if (previewResources_ && templateList_)
        {
            pendingRows.reserve(static_cast<std::size_t>(templateList_->count()));
            const QIcon loadingIcon(
                ::sed::ui::detail::makePreviewLoadingPixmap(
                    templateList_->iconSize()));
            for (int row = 0; row < templateList_->count(); ++row)
            {
                if (QListWidgetItem* item = templateList_->item(row))
                {
                    item->setIcon(loadingIcon);
                    item->setData(
                        ::sed::ui::detail::kPreviewThumbStateRole,
                        static_cast<int>(::sed::ui::detail::PreviewThumbState::loading));
                    pendingRows.push_back(row);
                }
            }
        }
        templateIconQueue_.reset(
            templateList_ ? templateList_->count() : 0,
            pendingRows);

        refreshPreview(false);
        scheduleTemplateIconLoad(0);
    }

    void scheduleTemplateIconLoad(const int delayMs)
    {
        if (!templateIconLoadTimer_ || templateIconQueue_.pendingCount() <= 0 || templateIconLoadSuspendDepth_ > 0)
        {
            return;
        }
        const auto kThumbTuning = ::sed::ui::detail::sharedModelThumbLoadTuning();
        if (!templateIconLoadTimer_->isActive())
        {
            templateIconLoadTimer_->start(std::max(kThumbTuning.startDelayMs, delayMs));
        }
    }

    bool visibleTemplateIconRowRange(int& rowBegin, int& rowEnd) const
    {
        return ::sed::ui::detail::visiblePreviewListRowRange(templateList_, rowBegin, rowEnd, 4);
    }

    void queueTemplateModelPrep(const std::string& modelName)
    {
        const QSize iconSize = templateList_
            ? templateList_->iconSize()
            : QSize(96, 96);
        const bool wireframe =
            loadTemplateListModelPreviewMode()
            == TemplateListModelPreviewMode::wireframe;
        (void)::sed::ui::detail::queueSharedPreviewModelBoxPrep(
            this,
            previewThumbnailResources_,
            modelName,
            previewMasterCmp_,
            iconSize,
            wireframe,
            templateIconPrepInFlight_,
            templateIconActiveJobs_,
            [this](const bool current)
            {
                (void)current;
                scheduleTemplateIconLoad(0);
            });
    }

    bool loadTemplateIconForRow(
        const int row,
        bool* renderedAuthoritativeThumbnail = nullptr)
    {
        if (renderedAuthoritativeThumbnail)
        {
            *renderedAuthoritativeThumbnail = false;
        }
        if (!templateList_ || row < 0 || row >= templateList_->count() || !templateIconQueue_.hasRow(row))
        {
            return false;
        }
        if (!templateIconQueue_.isPending(row))
        {
            return false;
        }

        QListWidgetItem* item = templateList_->item(row);
        if (!item)
        {
            return false;
        }

        const QString templateName = item->data(Qt::UserRole).toString().trimmed().isEmpty()
            ? item->text().trimmed()
            : item->data(Qt::UserRole).toString().trimmed();
        const ThingTemplate* tpl = templateListPreviewDb_.find(templateName.toStdString());
        const TemplateListModelPreviewMode thumbMode = loadTemplateListModelPreviewMode();
        bool assignedIcon = false;
        QIcon assignedPreviewIcon{};

        auto finishPendingRow = [&](const bool clearIcon)
        {
            if (clearIcon)
            {
                ::sed::ui::detail::publishPreviewThumbnailItem(
                    templateList_,
                    item,
                    QIcon(::sed::ui::detail::makePreviewUnavailablePixmap(
                        templateList_->iconSize())),
                    ::sed::ui::detail::kPreviewThumbStateRole,
                    static_cast<int>(::sed::ui::detail::PreviewThumbState::unavailable));
            }
            templateIconQueue_.finish(row);
        };

        if (!tpl)
        {
            finishPendingRow(true);
            return false;
        }

        if (previewResources_)
        {
            if (const std::optional<std::string> spriteName = resolvedTemplatePreviewField(*tpl, &templateListPreviewDb_, tpl->name, "sprite"))
            {
                watchTemplateSpritePreviewDependencies(*spriteName);
                const std::string cacheKey =
                    std::string("spr:")
                    + ::sed::text::upperAscii(*spriteName)
                    + "|deps="
                    + ::sed::ui::detail::sharedPreviewSpriteDependencyRevisionToken(
                        previewResources_, previewMasterCmp_, *spriteName);
                if (const auto it = previewIconCache_.find(cacheKey); it != previewIconCache_.end())
                {
                    assignedPreviewIcon = it->second;
                    assignedIcon = true;
                }
                else if (const std::optional<QImage> spriteImage = loadTemplateSpritePreviewImage(previewResources_, previewMasterCmp_, *spriteName))
                {
                    const QIcon icon(makeTemplatePreviewPixmap(*spriteImage, templateList_->iconSize()));
                    previewIconCache_[cacheKey] = icon;
                    assignedPreviewIcon = icon;
                    assignedIcon = true;
                }
            }
            if (!assignedIcon
                && thumbMode != TemplateListModelPreviewMode::none)
            {
                std::optional<std::string> modelName = resolvedTemplatePreviewField(*tpl, &templateListPreviewDb_, tpl->name, "model3d");
                if (!modelName)
                {
                    modelName = resolvedTemplatePreviewField(*tpl, &templateListPreviewDb_, tpl->name, "model3do");
                }
                if (modelName)
                {
                    watchTemplateModelPreviewDependencies(*modelName);
                    const QSize iconSize = templateList_->iconSize();
                    QIcon icon{};
                    const auto result = ::sed::ui::detail::loadSharedModelPreviewIcon(
                        previewIconCache_,
                        modelThumbnailRenderer_,
                        previewThumbnailResources_,
                        projectType_,
                        previewMasterCmp_,
                        *modelName,
                        iconSize,
                        thumbMode == TemplateListModelPreviewMode::wireframe,
                        templateIconActiveJobs_,
                        [this](const std::string& queuedModelName)
                        {
                            queueTemplateModelPrep(queuedModelName);
                        },
                        icon);
                    // The first call may have queued parsing. Register MATs as
                    // soon as that cache becomes available on a retry.
                    watchTemplateModelPreviewDependencies(*modelName);
                    switch (result)
                    {
                        case ::sed::ui::detail::SharedModelPreviewIconLoadResult::assigned:
                            assignedPreviewIcon = icon;
                            assignedIcon = true;
                            break;
                        case ::sed::ui::detail::SharedModelPreviewIconLoadResult::rendered:
                            assignedPreviewIcon = icon;
                            assignedIcon = true;
                            if (renderedAuthoritativeThumbnail)
                            {
                                *renderedAuthoritativeThumbnail = true;
                            }
                            break;
                        case ::sed::ui::detail::SharedModelPreviewIconLoadResult::pending:
                            return false;
                        case ::sed::ui::detail::SharedModelPreviewIconLoadResult::unavailable:
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

        ::sed::ui::detail::publishPreviewThumbnailItem(
            templateList_,
            item,
            assignedPreviewIcon,
            ::sed::ui::detail::kPreviewThumbStateRole,
            static_cast<int>(::sed::ui::detail::PreviewThumbState::ready));
        templateIconQueue_.finish(row);
        return true;
    }

    bool requeueVisibleLoadingTemplateIconRows()
    {
        if (!templateList_ || templateIconQueue_.empty())
        {
            return false;
        }

        int rowBegin = -1;
        int rowEnd = -1;
        if (!visibleTemplateIconRowRange(rowBegin, rowEnd))
        {
            return false;
        }

        const auto kThumbTuning = ::sed::ui::detail::sharedModelThumbLoadTuning();
        const int margin = kThumbTuning.visibleMargin;
        rowBegin = std::max(0, rowBegin - margin);
        rowEnd = std::min(templateList_->count() - 1, rowEnd + margin);

        bool repaired = false;
        for (int row = rowBegin; row <= rowEnd; ++row)
        {
            if (!templateIconQueue_.hasRow(row) || templateIconQueue_.isPending(row))
            {
                continue;
            }

            QListWidgetItem* item = templateList_->item(row);
            if (!item)
            {
                continue;
            }

            const auto state = static_cast<::sed::ui::detail::PreviewThumbState>(
                item->data(::sed::ui::detail::kPreviewThumbStateRole).toInt());
            if (state != ::sed::ui::detail::PreviewThumbState::loading)
            {
                continue;
            }

            repaired = templateIconQueue_.markPending(row) || repaired;
        }

        return repaired;
    }

    void processTemplateIconLoadBatch()
    {
        (void)requeueVisibleLoadingTemplateIconRows();
        if (templateIconPublicationPacer_.interactionActive())
        {
            scheduleTemplateIconLoad(
                templateIconPublicationPacer_.delayUntilQuiet());
            return;
        }
        if (templateIconLoadSuspendDepth_ > 0)
        {
            if (templateIconLoadTimer_)
            {
                templateIconLoadTimer_->stop();
            }
            return;
        }
        if (templateIconLoadInProgress_)
        {
            scheduleTemplateIconLoad(8);
            return;
        }
        if (!templateList_ || templateIconQueue_.pendingCount() <= 0)
        {
            if (templateIconLoadTimer_)
            {
                templateIconLoadTimer_->stop();
            }
            return;
        }

        struct TemplateIconBatchScope final
        {
            bool& flag;
            explicit TemplateIconBatchScope(bool& value)
                : flag(value)
            {
                flag = true;
            }
            ~TemplateIconBatchScope()
            {
                flag = false;
            }
        } scope(templateIconLoadInProgress_);

        const int itemCount = templateList_->count();
        if (itemCount <= 0)
        {
            clearTemplateIconPending();
            return;
        }

        const auto kThumbTuning = ::sed::ui::detail::sharedModelThumbLoadTuning();
        const int visibleBatchCount = kThumbTuning.visibleBatchCount;
        const int offscreenBatchCount = kThumbTuning.offscreenBatchCount;
        int loaded = 0;
        int processed = 0;
        bool visiblePending = false;
        bool renderedAuthoritativeThumbnail = false;
        QElapsedTimer batchTimer{};
        batchTimer.start();

        int rowBegin = -1;
        int rowEnd = -1;
        if (visibleTemplateIconRowRange(rowBegin, rowEnd))
        {
            const int margin = kThumbTuning.visibleMargin;
            rowBegin = std::max(0, rowBegin - margin);
            rowEnd = std::min(itemCount - 1, rowEnd + margin);
            const std::vector<int> priorityRows =
                ::sed::ui::detail::previewThumbnailPriorityRows(
                    rowBegin,
                    rowEnd,
                    templateList_->currentRow());
            for (const int row : priorityRows)
            {
                if (templateIconQueue_.isPending(row))
                {
                    visiblePending = true;
                }
                if (!templateIconQueue_.isPending(row)
                    || renderedAuthoritativeThumbnail
                    || !::sed::ui::detail::previewThumbnailBatchCanContinue(
                        processed,
                        visibleBatchCount,
                        batchTimer.nsecsElapsed()))
                {
                    continue;
                }
                bool renderedThisRow = false;
                ++processed;
                if (loadTemplateIconForRow(row, &renderedThisRow))
                {
                    ++loaded;
                }
                renderedAuthoritativeThumbnail =
                    renderedAuthoritativeThumbnail || renderedThisRow;
            }
        }

        int scanned = 0;
        while (!visiblePending
               && loaded < offscreenBatchCount
               && scanned < itemCount
               && templateIconQueue_.pendingCount() > 0)
        {
            const int row = templateIconQueue_.nextScanRow(itemCount);
            if (loadTemplateIconForRow(row))
            {
                ++loaded;
            }
            ++scanned;
        }

        if (visiblePending && templateIconQueue_.pendingCount() > 0)
        {
            if (::sed::ui::detail::previewThumbnailQueueNeedsPolling(
                    true, templateIconActiveJobs_))
            {
                scheduleTemplateIconLoad(kThumbTuning.visibleDelayMs);
            }
            else if (templateIconLoadTimer_)
            {
                templateIconLoadTimer_->stop();
            }
        }
        else if (templateIconLoadTimer_)
        {
            templateIconLoadTimer_->stop();
        }
    }

    TemplateListSortMode templateListSortMode() const noexcept
    {
        return templateSortCombo_
            ? static_cast<TemplateListSortMode>(
                templateSortCombo_->currentData().toInt())
            : TemplateListSortMode::nameAscending;
    }

    std::vector<std::size_t> templateDisplayOrder() const
    {
        std::vector<std::size_t> order = ui::templateCategorySourceDisplayOrder(
            templates_, currentTemplateCategoryNames());
        const TemplateListSortMode mode = templateListSortMode();
        if (mode == TemplateListSortMode::sourceOrder)
        {
            return order;
        }
        std::stable_sort(order.begin(), order.end(),
            [this, mode](const std::size_t lhs, const std::size_t rhs)
            {
                const QString left = QString::fromStdString(templates_[lhs].name);
                const QString right = QString::fromStdString(templates_[rhs].name);
                const int compared = left.compare(right, Qt::CaseInsensitive);
                return mode == TemplateListSortMode::nameAscending
                    ? compared < 0
                    : compared > 0;
            });
        return order;
    }

    int templateRowForSourceIndex(const int sourceIndex) const noexcept
    {
        if (!templateList_ || sourceIndex < 0)
        {
            return -1;
        }
        for (int row = 0; row < templateList_->count(); ++row)
        {
            const QListWidgetItem* item = templateList_->item(row);
            if (item && item->data(kTemplateSourceIndexRole).toInt() == sourceIndex)
            {
                return row;
            }
        }
        return -1;
    }

    void refreshTemplateList(const int desiredIndex)
    {
        loading_ = true;
        clearTemplateIconPending();
        templateList_->clear();
        templateListPreviewDb_.clear();
        const std::vector<std::size_t> displayOrder = templateDisplayOrder();
        for (const ThingTemplate& tpl : templates_)
        {
            try
            {
                (void)templateListPreviewDb_.addOrUpdate(tpl, true);
            }
            catch (const std::exception&)
            {
            }
        }

        std::vector<int> pendingRows{};
        pendingRows.reserve(templates_.size());
        const QIcon initialListIcon(
            previewResources_
                ? ::sed::ui::detail::makePreviewLoadingPixmap(
                    templateList_->iconSize())
                : ::sed::ui::detail::makePreviewUnavailablePixmap(
                    templateList_->iconSize()));

        for (const std::size_t sourceIndex : displayOrder)
        {
            const ThingTemplate& tpl = templates_[sourceIndex];
            const QString name = QString::fromStdString(tpl.name);
            const QString parent = QString::fromStdString(tpl.parent).trimmed();
            const std::optional<std::string> typeValue =
                resolvedTemplatePreviewField(tpl, &templateListPreviewDb_, tpl.name, "type");
            const QString thingType = typeValue ? QString::fromStdString(*typeValue).trimmed() : QString{};
            QString displayName = parent.isEmpty() || parent.compare("none", Qt::CaseInsensitive) == 0
                ? name
                : QString("%1  [%2]").arg(name, parent);
            if (!thingType.isEmpty())
            {
                displayName += QStringLiteral("\n[%1]").arg(thingType);
            }
            auto* item = new QListWidgetItem(displayName, templateList_);
            item->setData(Qt::UserRole, name);
            item->setData(kTemplateSourceIndexRole,
                static_cast<qulonglong>(sourceIndex));
            item->setTextAlignment(Qt::AlignHCenter);
            item->setSizeHint(QSize(132, 158));
            item->setIcon(initialListIcon);
            item->setData(
                ::sed::ui::detail::kPreviewThumbStateRole,
                static_cast<int>(
                    previewResources_
                        ? ::sed::ui::detail::PreviewThumbState::loading
                        : ::sed::ui::detail::PreviewThumbState::unavailable));
            QString tooltip = parent.isEmpty() || parent.compare("none", Qt::CaseInsensitive) == 0
                ? QString::fromStdString(tpl.desc)
                : QString("%1\nParent: %2").arg(QString::fromStdString(tpl.desc), parent);
            if (!thingType.isEmpty())
            {
                tooltip = tooltip.trimmed().isEmpty()
                    ? QStringLiteral("Thing Type: %1").arg(thingType)
                    : tooltip + QStringLiteral("\nThing Type: %1").arg(thingType);
            }
            item->setToolTip(tooltip.trimmed());
            // Defer preview-field resolution until the row is actually visible.
            // This keeps large template lists from paying the inherited sprite/model lookup cost up front.
            if (previewResources_)
            {
                pendingRows.push_back(templateList_->count() - 1);
            }
        }
        applyTemplateFilter();
        loading_ = false;

        templateIconQueue_.reset(templateList_->count(), pendingRows);
        scheduleTemplateIconLoad(0);
        QTimer::singleShot(48, this, [this]()
            {
                if (templateIconQueue_.pendingCount() > 0)
                {
                    scheduleTemplateIconLoad(0);
                }
            });

        int nextIndex = desiredIndex;
        if (nextIndex < 0 && !templates_.empty())
        {
            nextIndex = 0;
        }
        if (nextIndex >= static_cast<int>(templates_.size()))
        {
            nextIndex = static_cast<int>(templates_.size()) - 1;
        }

        currentIndex_ = nextIndex;
        templateList_->setCurrentRow(templateRowForSourceIndex(nextIndex));
        applyTemplateFilter();
        loadCurrentTemplate();
        scheduleTemplateIconLoad(0);
    }

    void applyTemplateFilter()
    {
        if (!templateList_)
        {
            return;
        }

        const QString needle = searchEdit_ ? searchEdit_->text().trimmed() : QString{};
        const QStringList categoryNames = currentTemplateCategoryNames();
        const bool restrictCategory = !categoryNames.isEmpty();
        std::vector<ui::TemplateFilterCandidate> candidates{};
        candidates.reserve(static_cast<std::size_t>(templateList_->count()));
        for (int row = 0; row < templateList_->count(); ++row)
        {
            QListWidgetItem* item = templateList_->item(row);
            if (!item)
            {
                candidates.push_back(ui::TemplateFilterCandidate{
                    QString{}, QString{}, QString{}, false });
                continue;
            }

            const QString actualName = item->data(Qt::UserRole).toString().trimmed();
            bool matchesCategory = true;
            if (restrictCategory)
            {
                matchesCategory = false;
                for (const QString& categoryName : categoryNames)
                {
                    if (categoryName.compare(actualName, Qt::CaseInsensitive) == 0)
                    {
                        matchesCategory = true;
                        break;
                    }
                }
            }

            candidates.push_back(ui::TemplateFilterCandidate{
                actualName,
                item->text(),
                item->toolTip(),
                matchesCategory,
            });
        }

        const ui::TemplateFilterResult result = ui::filterTemplateCandidates(
            candidates, needle, templateList_->currentRow());
        for (int row = 0; row < templateList_->count(); ++row)
        {
            if (QListWidgetItem* item = templateList_->item(row))
            {
                item->setHidden(!result.visibleRows[static_cast<std::size_t>(row)]);
            }
        }
        if (templateList_->currentRow() != result.preferredRow)
        {
            templateList_->setCurrentRow(result.preferredRow);
        }

        templateList_->doItemsLayout();
        templateList_->viewport()->update();
    }

    void applyFieldFilter()
    {
        if (!fieldsTable_)
        {
            return;
        }

        const QString needle = fieldSearchEdit_
            ? fieldSearchEdit_->text().trimmed()
            : QString{};
        for (int row = 0; row < fieldsTable_->rowCount(); ++row)
        {
            bool matches = needle.isEmpty();
            for (int column = 0; !matches && column < fieldsTable_->columnCount(); ++column)
            {
                if (const QTableWidgetItem* item = fieldsTable_->item(row, column))
                {
                    matches = item->text().contains(needle, Qt::CaseInsensitive);
                }
            }
            fieldsTable_->setRowHidden(row, !matches);
        }
    }

    QString templateParameterValueHelp(const TplValue& value) const
    {
        QString help{};
        if (value.type == ValueType::str || value.type == ValueType::unknown)
        {
            if (value.adjType == AdjType::unk)
            {
                help = QStringLiteral(
                    "Stored as text in the template file. The engine interprets recognized parameter names (including numeric values) according to its own rules, and SED preserves the authored text.");
            }
            else
            {
                const std::string_view adjTypeName = ::sed::toString(value.adjType);
                help = QStringLiteral("Stored as a resource token (%1). SED preserves the authored token text.")
                    .arg(QString::fromLatin1(
                        adjTypeName.data(), static_cast<qsizetype>(adjTypeName.size())));
            }
        }
        else
        {
            const std::string_view type = ::sed::toString(value.type);
            help = QStringLiteral("Stored as a typed %1 value.").arg(QString::fromLatin1(
                type.data(), static_cast<qsizetype>(type.size())));
        }

        const QString description = templateParameterDescription(projectType_, value.name);
        if (!description.isEmpty())
        {
            help += QLatin1Char(' ') + description;
        }
        return help;
    }

    void scheduleInlineTemplateParameterRefresh(
        const int templateIndex,
        const std::optional<std::size_t> selectedLocalIndex,
        const QPointer<QLineEdit> activeInlineEdit)
    {
        if (templateIndex < 0
            || static_cast<std::size_t>(templateIndex) >= templates_.size())
        {
            return;
        }

        const std::string templateName = templates_[static_cast<std::size_t>(templateIndex)].name;
        const std::uint64_t generation = ++inlineTemplateRefreshGeneration_;
        QTimer::singleShot(
            0,
            this,
            [this, templateIndex, templateName, selectedLocalIndex, generation, activeInlineEdit]()
            {
                if (generation != inlineTemplateRefreshGeneration_
                    || templateIndex != currentIndex_
                    || !hasCurrentTemplate()
                    || templates_[static_cast<std::size_t>(templateIndex)].name != templateName)
                {
                    return;
                }

                // Return is intercepted to keep the dialog's Save default from
                // accepting the whole editor. Move focus off the still-active
                // line edit before rebuilding its owning table; QTableWidget
                // row removal must never destroy the focused editor directly.
                if (activeInlineEdit && activeInlineEdit->hasFocus())
                {
                    fieldsTable_->setFocus(Qt::OtherFocusReason);
                }
                loadCurrentTemplate(false);
                if (!selectedLocalIndex)
                {
                    return;
                }
                const auto displayed = std::find_if(
                    effectiveFieldRows_.begin(),
                    effectiveFieldRows_.end(),
                    [selectedLocalIndex](const ui::EffectiveTemplateParameter& current)
                    {
                        return current.localIndex && *current.localIndex == *selectedLocalIndex;
                    });
                if (displayed != effectiveFieldRows_.end())
                {
                    fieldsTable_->selectRow(static_cast<int>(
                        std::distance(effectiveFieldRows_.begin(), displayed)));
                }
            });
    }

    bool commitInlineTemplateParameter(
        const int templateIndex,
        const ui::EffectiveTemplateParameter& parameter,
        const QString& editedText,
        const QPointer<QLineEdit> activeInlineEdit)
    {
        if (loading_ || templateIndex != currentIndex_ || !hasCurrentTemplate())
        {
            return false;
        }

        ThingTemplate& tpl = templates_[static_cast<std::size_t>(templateIndex)];
        ui::EffectiveTemplateParameter currentParameter = parameter;
        const std::vector<ui::EffectiveTemplateParameter> currentParameters =
            ui::effectiveTemplateParameters(
                templates_, static_cast<std::size_t>(templateIndex), projectType_);
        const auto currentRow = std::find_if(
            currentParameters.begin(),
            currentParameters.end(),
            [&parameter](const ui::EffectiveTemplateParameter& current)
            {
                if (parameter.localIndex)
                {
                    return current.localIndex == parameter.localIndex;
                }
                return !parameter.repeatable
                    && !current.repeatable
                    && sed::text::iequalsAscii(current.value.name, parameter.value.name);
            });
        if (currentRow != currentParameters.end())
        {
            currentParameter = *currentRow;
        }

        if (currentParameter.localIndex)
        {
            if (*currentParameter.localIndex >= tpl.values.size()
                || !sed::text::iequalsAscii(
                    tpl.values[*currentParameter.localIndex].name, currentParameter.value.name))
            {
                return false;
            }
        }
        else if (findTemplateValueIndex(tpl, currentParameter.value.name))
        {
            return false;
        }

        const std::optional<TplValue> parsed = ui::parseTemplateParameterValue(
            QString::fromStdString(currentParameter.value.name), editedText, &currentParameter.value);
        if (!parsed)
        {
            return false;
        }
        if (parsed->type == currentParameter.value.type
            && parsed->adjType == currentParameter.value.adjType
            && parsed->asString() == currentParameter.value.asString())
        {
            return true;
        }

        const std::size_t insertIndex = thing::parameters::applyTemplateParameterEdit(
            tpl, *parsed, currentParameter.localIndex, currentParameter.repeatable);
        if (sed::text::iequalsAscii(parsed->name, "frame")
            || sed::text::iequalsAscii(parsed->name, "numframes"))
        {
            std::size_t selectedOccurrence = 0;
            for (std::size_t index = 0; index < insertIndex; ++index)
            {
                if (sed::text::iequalsAscii(tpl.values[index].name, parsed->name))
                {
                    ++selectedOccurrence;
                }
            }

            sed::level::thingSyncNumFrames(tpl);
            std::size_t currentOccurrence = 0;
            int restoredIndex = -1;
            for (std::size_t index = 0; index < tpl.values.size(); ++index)
            {
                if (!sed::text::iequalsAscii(tpl.values[index].name, parsed->name))
                {
                    continue;
                }
                if (currentOccurrence++ == selectedOccurrence)
                {
                    restoredIndex = static_cast<int>(index);
                    break;
                }
            }
            scheduleInlineTemplateParameterRefresh(
                templateIndex,
                restoredIndex >= 0
                    ? std::optional<std::size_t>{ static_cast<std::size_t>(restoredIndex) }
                    : std::nullopt,
                activeInlineEdit);
            return true;
        }

        scheduleInlineTemplateParameterRefresh(
            templateIndex, insertIndex, activeInlineEdit);
        return true;
    }

    void reloadEffectiveParameterTable()
    {
        fieldsTable_->setRowCount(0);
        effectiveFieldRows_.clear();
        if (!hasCurrentTemplate())
        {
            applyFieldFilter();
            updateFieldButtonState();
            return;
        }

        effectiveFieldRows_ = ui::effectiveTemplateParameters(
            templates_, static_cast<std::size_t>(currentIndex_), projectType_);
        for (const ui::EffectiveTemplateParameter& parameter : effectiveFieldRows_)
        {
            const TplValue& value = parameter.value;
            const int row = fieldsTable_->rowCount();
            fieldsTable_->insertRow(row);

            auto* nameItem = new QTableWidgetItem(QString::fromStdString(value.name));
            QString sourceLabel = QStringLiteral("Local");
            QString sourceTip = QStringLiteral("Defined on this template.");
            if (parameter.localIndex && parameter.inheritedValue)
            {
                sourceLabel = QStringLiteral("Local override: %1")
                    .arg(QString::fromStdString(parameter.inheritedFrom));
                sourceTip = QStringLiteral("Overrides the value inherited from %1. Restore removes the local override.")
                    .arg(QString::fromStdString(parameter.inheritedFrom));
            }
            else if (!parameter.localIndex)
            {
                sourceLabel = QStringLiteral("Inherited: %1")
                    .arg(QString::fromStdString(parameter.inheritedFrom));
                sourceTip = parameter.repeatable
                    ? QStringLiteral("Inherited append-only value from %1. Add a local occurrence to extend it.")
                          .arg(QString::fromStdString(parameter.inheritedFrom))
                    : QStringLiteral("Inherited from %1. Editing creates a local override.")
                          .arg(QString::fromStdString(parameter.inheritedFrom));
            }
            auto* sourceItem = new QTableWidgetItem(sourceLabel);
            auto* valueItem = new QTableWidgetItem(QString::fromStdString(value.asString()));
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            sourceItem->setFlags(sourceItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
            nameItem->setToolTip(sourceTip);
            sourceItem->setToolTip(sourceTip);
            const QString valueHelp = templateParameterValueHelp(value);
            valueItem->setToolTip(sourceTip + QLatin1Char(' ') + valueHelp);
            fieldsTable_->setItem(row, 0, nameItem);
            fieldsTable_->setItem(row, 1, sourceItem);
            fieldsTable_->setItem(row, 2, valueItem);

            auto* valueCell = new QWidget(fieldsTable_);
            valueCell->setObjectName(QStringLiteral("templateParameterInlineValueCell"));
            valueCell->setToolTip(sourceTip + QLatin1Char(' ') + valueHelp);
            auto* valueLayout = new QHBoxLayout(valueCell);
            valueLayout->setContentsMargins(2, 0, 2, 0);
            valueLayout->setSpacing(4);
            auto* valueEdit = new QLineEdit(valueCell);
            valueEdit->setObjectName(QStringLiteral("templateParameterInlineValueEdit"));
            const ValueType numericType = ui::typedNumericTextType(value);
            const QByteArray displayBaselineProperty("template.numeric.displayBaseline");
            ui::configureTypedNumericLineEdit(*valueEdit, QString::fromStdString(value.asString()),
                numericType, displayBaselineProperty);
            valueEdit->setProperty(displayBaselineProperty.constData(), valueEdit->text());
            valueEdit->setToolTip(valueCell->toolTip());
            const bool inheritedRepeatable = parameter.repeatable && !parameter.localIndex;
            valueEdit->setReadOnly(inheritedRepeatable);
            if (!inheritedRepeatable)
            {
                valueEdit->installEventFilter(this);
            }
            if (!parameter.localIndex)
            {
                QPalette palette = valueEdit->palette();
                palette.setColor(QPalette::Text, QColor(QStringLiteral("#9da8b8")));
                valueEdit->setPalette(palette);
            }
            valueLayout->addWidget(valueEdit, 1);

            const std::string fieldName = value.name;
            const std::string effectiveType = typeBox_
                ? typeBox_->currentText().trimmed().toStdString()
                : std::string{};
            const thing::parameter_editor::ValueEditorInfo editorInfo =
                thing::parameter_editor::valueEditorInfo(
                    projectType_, fieldName, effectiveType);
            const AdjType adjType = value.adjType != AdjType::unk
                ? value.adjType
                : ::sed::tplAdjTypeForName(fieldName);
            const bool hasValuePicker = isStructuredLightParameter(fieldName)
                || editorInfo.kind != thing::parameter_editor::ValueEditorKind::none
                || adjType != AdjType::unk;
            if (hasValuePicker && !inheritedRepeatable)
            {
                auto* valuePicker = new QToolButton(valueCell);
                valuePicker->setObjectName(QStringLiteral("templateParameterInlineValuePicker"));
                valuePicker->setText(QStringLiteral("…"));
                valuePicker->setAccessibleName(
                    QStringLiteral("Choose value for %1").arg(QString::fromStdString(fieldName)));
                valuePicker->setFocusPolicy(Qt::NoFocus);
                valuePicker->setToolTip(isStructuredLightParameter(fieldName)
                    ? QStringLiteral("Open the light value editor.")
                    : (editorInfo.kind != thing::parameter_editor::ValueEditorKind::none
                        ? editorInfo.toolTip
                        : QStringLiteral("Choose a resource for this parameter.")));
                valuePicker->setEnabled(!inheritedRepeatable);
                valueLayout->addWidget(valuePicker);
                connect(valuePicker, &QToolButton::clicked, this, [this, row, valuePicker]()
                    {
                        fieldsTable_->selectRow(row);
                        browseSelectedFieldValue(valuePicker);
                    });
            }

            const ui::EffectiveTemplateParameter parameterSnapshot = parameter;
            const int templateIndexSnapshot = currentIndex_;
            const QString originalValue = QString::fromStdString(value.asString());
            const QPointer<QLineEdit> safeValueEdit(valueEdit);
            connect(valueEdit, &QLineEdit::textEdited, this, [this, row, safeValueEdit](const QString&)
                {
                    if (safeValueEdit)
                    {
                        safeValueEdit->setModified(true);
                        safeValueEdit->setStyleSheet({});
                        fieldsTable_->selectRow(row);
                    }
                });
            connect(valueEdit, &QLineEdit::editingFinished, this,
                [this, templateIndexSnapshot, parameterSnapshot, originalValue, numericType,
                    displayBaselineProperty, safeValueEdit]()
                {
                    if (!safeValueEdit)
                    {
                        return;
                    }
                    const QString editedText = safeValueEdit->text();
                    if (editedText == safeValueEdit->property(displayBaselineProperty.constData()).toString())
                    {
                        ui::setTypedNumericLineEditText(*safeValueEdit,
                            safeValueEdit->property("sed.typedNumericCanonicalText").toString(),
                            numericType, displayBaselineProperty);
                        return;
                    }
                    const auto canonical = ui::canonicalTypedNumericText(
                        editedText, numericType, safeValueEdit->locale());
                    if (!canonical || !commitInlineTemplateParameter(
                            templateIndexSnapshot,
                            parameterSnapshot,
                            *canonical,
                            safeValueEdit))
                    {
                        ui::setTypedNumericLineEditText(*safeValueEdit, originalValue,
                            numericType, displayBaselineProperty);
                        safeValueEdit->setStyleSheet(
                            QStringLiteral("QLineEdit { border:1px solid #d76b6b; }"));
                        safeValueEdit->setToolTip(
                            QStringLiteral("Invalid value. The previous parameter was kept."));
                        return;
                    }
                    ui::setTypedNumericLineEditText(*safeValueEdit, *canonical,
                        numericType, displayBaselineProperty);
                });
            fieldsTable_->setCellWidget(row, 2, valueCell);

            if (!parameter.localIndex)
            {
                const QBrush inheritedBrush(QColor(QStringLiteral("#9da8b8")));
                nameItem->setForeground(inheritedBrush);
                sourceItem->setForeground(inheritedBrush);
                valueItem->setForeground(inheritedBrush);
            }
        }

        applyFieldFilter();
        updateFieldButtonState();
    }

    void loadCurrentTemplate(const bool refitPreview = true)
    {
        ++inlineTemplateRefreshGeneration_;
        loading_ = true;

        const bool enabled = hasCurrentTemplate();
        detailPanel_->setEnabled(enabled);
        if (!enabled)
        {
            nameEdit_->clear();
            parentEdit_->clear();
            descEdit_->clear();
            for (ui::ScrubFloatEdit* spin : bboxSpins_)
            {
                spin->setValue(0.0);
            }
            reloadEffectiveParameterTable();
            loading_ = false;
            refreshPreview(refitPreview);
            return;
        }

        const ThingTemplate& tpl = templates_[static_cast<std::size_t>(currentIndex_)];
        nameEdit_->setText(QString::fromStdString(tpl.name));
        parentEdit_->setText(QString::fromStdString(tpl.parent.empty() ? std::string("none") : tpl.parent));
        descEdit_->setText(QString::fromStdString(tpl.desc));
        if (const TplValue* typeValue = templateListPreviewDb_.findField(tpl.name, "type"))
        {
            typeBox_->setCurrentText(QString::fromStdString(typeValue->asString()));
        }
        else if (const std::optional<std::size_t> typeIndex = findTemplateValueIndex(tpl, "type"))
        {
            typeBox_->setCurrentText(QString::fromStdString(tpl.values[*typeIndex].asString()));
        }
        else
        {
            typeBox_->setCurrentText("free");
        }
        bboxSpins_[0]->setValue(tpl.bbox.x1);
        bboxSpins_[1]->setValue(tpl.bbox.y1);
        bboxSpins_[2]->setValue(tpl.bbox.z1);
        bboxSpins_[3]->setValue(tpl.bbox.x2);
        bboxSpins_[4]->setValue(tpl.bbox.y2);
        bboxSpins_[5]->setValue(tpl.bbox.z2);

        reloadEffectiveParameterTable();

        loading_ = false;
        refreshPreview(refitPreview);
    }

    void syncBasicFieldsFromWidgets()
    {
        if (loading_ || !hasCurrentTemplate())
        {
            return;
        }

        ThingTemplate& tpl = templates_[static_cast<std::size_t>(currentIndex_)];
        tpl.name = nameEdit_->text().trimmed().toStdString();
        tpl.parent = parentEdit_->text().trimmed().toStdString();
        tpl.desc = descEdit_->text().trimmed().toStdString();
        // Bounds are updated only by their own valueChanged handlers. Reading
        // every displayed spin box here would round/clamp untouched authorship.

        const QString typeText = typeBox_->currentText().trimmed();
        if (typeText.isEmpty() || typeText.compare("none", Qt::CaseInsensitive) == 0)
        {
            tpl.values.erase(
                std::remove_if(tpl.values.begin(), tpl.values.end(), [](const TplValue& value)
                    {
                        return ::sed::text::iequalsAscii(value.name, "type");
                    }),
                tpl.values.end());
        }
        else if (const std::optional<TplValue> parsedType =
                     ui::parseTemplateParameterValue("type", typeText))
        {
            if (const std::optional<std::size_t> typeIndex = findTemplateValueIndex(tpl, "type"))
            {
                tpl.values[*typeIndex] = *parsedType;
            }
            else
            {
                tpl.values.push_back(*parsedType);
            }
        }

        if (QListWidgetItem* item = templateList_->currentItem())
        {
            const QString name = QString::fromStdString(tpl.name);
            const QString parent = QString::fromStdString(tpl.parent).trimmed();
            item->setData(Qt::UserRole, name);
            item->setText(parent.isEmpty() || parent.compare("none", Qt::CaseInsensitive) == 0
                    ? name
                    : QString("%1  [%2]").arg(name, parent));
            item->setToolTip(QString::fromStdString(tpl.desc));
        }

        refreshPreview(false);
    }

    int selectedFieldRow() const noexcept
    {
        const QModelIndexList selection = fieldsTable_->selectionModel() ? fieldsTable_->selectionModel()->selectedRows() : QModelIndexList{};
        return selection.isEmpty() ? -1 : selection.front().row();
    }

    const ui::EffectiveTemplateParameter* selectedEffectiveField() const noexcept
    {
        const int row = selectedFieldRow();
        return row >= 0 && static_cast<std::size_t>(row) < effectiveFieldRows_.size()
            ? &effectiveFieldRows_[static_cast<std::size_t>(row)]
            : nullptr;
    }

    void updateFieldButtonState()
    {
        const ui::EffectiveTemplateParameter* parameter = selectedEffectiveField();
        const bool selected = parameter != nullptr;
        const bool inheritedRepeatable = selected
            && parameter->repeatable && !parameter->localIndex;
        if (editFieldButton_)
        {
            editFieldButton_->setEnabled(selected && !inheritedRepeatable);
        }
        if (removeFieldButton_)
        {
            const bool restorable = selected && parameter->localIndex
                && parameter->inheritedValue && !parameter->repeatable;
            removeFieldButton_->setText(restorable
                    ? QStringLiteral("Restore Parent Value")
                    : QStringLiteral("Remove Parameter"));
            removeFieldButton_->setToolTip(restorable
                    ? QStringLiteral("Remove this template's override and reveal the inherited parent value.")
                    : (selected && !parameter->localIndex
                        ? QStringLiteral("Inherited parameters cannot be removed from this template.")
                        : QStringLiteral("Remove the local template parameter.")));
            removeFieldButton_->setEnabled(selected && parameter->localIndex.has_value());
        }
    }

    void createTemplate()
    {
        ThingTemplate tpl{};
        tpl.name = makeUniqueTemplateName("new_template", templates_).toStdString();
        tpl.parent = "none";
        templates_.push_back(std::move(tpl));
        refreshTemplateList(static_cast<int>(templates_.size()) - 1);
    }

    void duplicateCurrentTemplate()
    {
        if (!hasCurrentTemplate())
        {
            createTemplate();
            return;
        }

        ThingTemplate copy = templates_[static_cast<std::size_t>(currentIndex_)];
        copy.name = makeUniqueTemplateName(QString::fromStdString(copy.name) + "_copy", templates_).toStdString();
        templates_.insert(templates_.begin() + currentIndex_ + 1, std::move(copy));
        refreshTemplateList(currentIndex_ + 1);
    }

    void deleteCurrentTemplate()
    {
        if (!hasCurrentTemplate())
        {
            return;
        }

        const QString templateName = QString::fromStdString(
            templates_[static_cast<std::size_t>(currentIndex_)].name);
        QMessageBox confirmation(
            QMessageBox::Question,
            QStringLiteral("Delete Template"),
            QStringLiteral("Delete the selected template?"),
            QMessageBox::Yes | QMessageBox::No,
            this);
        confirmation.setProperty(
            kMessageBoxLoggingDestructiveProperty, true);
        confirmation.setProperty(kMessageBoxLoggingContextProperty,
            QStringLiteral("Template: %1").arg(templateName));
        if (confirmation.exec() != QMessageBox::Yes)
        {
            return;
        }

        templates_.erase(templates_.begin() + currentIndex_);
        refreshTemplateList(currentIndex_);
    }

    void editField(const int row)
    {
        if (!hasCurrentTemplate())
        {
            return;
        }

        ThingTemplate& tpl = templates_[static_cast<std::size_t>(currentIndex_)];
        std::optional<TplValue> initial{};
        const ui::EffectiveTemplateParameter* selected =
            row >= 0 && static_cast<std::size_t>(row) < effectiveFieldRows_.size()
            ? &effectiveFieldRows_[static_cast<std::size_t>(row)]
            : nullptr;
        if (selected && selected->repeatable && !selected->localIndex)
        {
            return;
        }
        if (selected)
        {
            initial = selected->value;
        }

        TemplateFieldDialog dialog(
            buildTemplateFieldNameOptions(projectType_, templates_),
            projectType_,
            typeBox_ ? typeBox_->currentText().trimmed() : QStringLiteral("free"),
            [this]()
            {
                return templateNameOptions(currentIndex_);
            },
            [this]() -> const std::vector<ThingTemplate>*
            {
                return &templates_;
            },
            [this](
                const AdjType adjType,
                const QString& fieldName,
                const QString& currentValue,
                const std::vector<ThingTemplate>* templateOverrides) -> std::optional<QString>
            {
                return browseResourcePaused(adjType, fieldName, currentValue, templateOverrides);
            },
            initial,
            selected && !selected->localIndex,
            this);
        if (dialog.exec() != QDialog::Accepted)
        {
            return;
        }

        const std::optional<TplValue> updated = dialog.field();
        if (!updated)
        {
            return;
        }

        const std::optional<std::size_t> editedIndex =
            selected ? selected->localIndex : std::nullopt;
        const bool repeatable = thing::parameter_schema::isRepeatableParameter(
            projectType_, updated->name);
        int insertRow = static_cast<int>(
            thing::parameters::applyTemplateParameterEdit(
                tpl, *updated, editedIndex, repeatable));
        const bool frameStructureChanged =
            sed::text::iequalsAscii(updated->name, "frame")
            || sed::text::iequalsAscii(updated->name, "numframes")
            || (initial
                && (sed::text::iequalsAscii(initial->name, "frame")
                    || sed::text::iequalsAscii(initial->name, "numframes")));
        if (frameStructureChanged)
        {
            std::size_t selectedOccurrence = 0;
            for (int index = 0; index < insertRow; ++index)
            {
                selectedOccurrence += static_cast<std::size_t>(
                    sed::text::iequalsAscii(
                        tpl.values[static_cast<std::size_t>(index)].name,
                        updated->name));
            }
            sed::level::thingSyncNumFrames(tpl);
            std::size_t currentOccurrence = 0;
            insertRow = -1;
            for (std::size_t index = 0; index < tpl.values.size(); ++index)
            {
                if (!sed::text::iequalsAscii(
                        tpl.values[index].name, updated->name))
                {
                    continue;
                }
                if (currentOccurrence++ == selectedOccurrence)
                {
                    insertRow = static_cast<int>(index);
                    break;
                }
            }
        }

        loadCurrentTemplate();
        const auto displayed = std::find_if(
            effectiveFieldRows_.begin(), effectiveFieldRows_.end(), [insertRow](const ui::EffectiveTemplateParameter& parameter)
            {
                return parameter.localIndex
                    && *parameter.localIndex == static_cast<std::size_t>(insertRow);
            });
        if (displayed != effectiveFieldRows_.end())
        {
            fieldsTable_->selectRow(static_cast<int>(
                std::distance(effectiveFieldRows_.begin(), displayed)));
        }
    }

    void browseSelectedFieldValue(QWidget* anchor = nullptr)
    {
        if (!hasCurrentTemplate())
        {
            return;
        }

        const int row = selectedFieldRow();
        const ui::EffectiveTemplateParameter* selected = selectedEffectiveField();
        if (!selected || (selected->repeatable && !selected->localIndex))
        {
            return;
        }

        ThingTemplate& tpl = templates_[static_cast<std::size_t>(currentIndex_)];
        const TplValue value = selected->value;
        const std::optional<std::size_t> editedIndex = selected->localIndex;
        const auto commit = [this, &tpl, editedIndex, row](const QString& picked, const TplValue& source)
        {
            const std::optional<TplValue> parsed = ui::parseTemplateParameterValue(
                QString::fromStdString(source.name), picked, &source);
            if (!parsed)
            {
                return;
            }
            const std::size_t localIndex = thing::parameters::applyTemplateParameterEdit(
                tpl, *parsed, editedIndex, false);
            loadCurrentTemplate();
            const auto displayed = std::find_if(
                effectiveFieldRows_.begin(), effectiveFieldRows_.end(), [localIndex](const ui::EffectiveTemplateParameter& parameter)
                {
                    return parameter.localIndex && *parameter.localIndex == localIndex;
                });
            fieldsTable_->selectRow(displayed == effectiveFieldRows_.end()
                    ? row
                    : static_cast<int>(std::distance(effectiveFieldRows_.begin(), displayed)));
        };

        if (isStructuredLightParameter(value.name))
        {
            if (const std::optional<QString> picked = editStructuredLightParameter(
                    this,
                    projectType_,
                    value.name,
                    QString::fromStdString(value.asString()),
                    anchor))
            {
                commit(*picked, value);
            }
            return;
        }

        const thing::parameter_editor::ValueEditorInfo editorInfo =
            thing::parameter_editor::valueEditorInfo(
                projectType_,
                value.name,
                typeBox_ ? typeBox_->currentText().trimmed().toStdString() : std::string{});
        if (editorInfo.kind != thing::parameter_editor::ValueEditorKind::none)
        {
            const std::optional<QString> picked = thing::parameter_editor::openValueEditor(
                this,
                projectType_,
                value.name,
                typeBox_ ? typeBox_->currentText().trimmed().toStdString() : std::string{},
                QString::fromStdString(value.asString()),
                anchor);
            if (!picked)
            {
                return;
            }
            commit(*picked, value);
            return;
        }

        const AdjType adjType = value.adjType != AdjType::unk
            ? value.adjType
            : ::sed::tplAdjTypeForName(value.name);
        if (adjType == AdjType::unk)
        {
            return;
        }

        if (adjType == AdjType::tpl)
        {
            if (browseResource_)
            {
                const std::optional<QString> picked = browseResourcePaused(
                    adjType,
                    QString::fromStdString(value.name),
                    QString::fromStdString(value.asString()),
                    &templates_);
                if (!picked)
                {
                    return;
                }

                commit(*picked, value);
                return;
            }

            QStringList names = templateNameOptions(currentIndex_);
            if (!names.contains("none", Qt::CaseInsensitive))
            {
                names.push_front("none");
            }

            const qsizetype currentIndex = names.indexOf(QString::fromStdString(value.asString()));
            bool ok = false;
            const QString picked = QInputDialog::getItem(
                this,
                "Choose Template",
                "Template",
                names,
                currentIndex < 0 ? 0 : static_cast<int>(currentIndex),
                false,
                &ok);
            if (!ok || picked.isEmpty())
            {
                return;
            }

            commit(picked, value);
            return;
        }
        else if (browseResource_)
        {
            const std::optional<QString> picked = browseResourcePaused(
                adjType,
                QString::fromStdString(value.name),
                QString::fromStdString(value.asString()),
                &templates_);
            if (!picked)
            {
                return;
            }

            commit(*picked, value);
        }
    }

    void removeSelectedField()
    {
        if (!hasCurrentTemplate())
        {
            return;
        }

        const int row = selectedFieldRow();
        const ui::EffectiveTemplateParameter* selected = selectedEffectiveField();
        ThingTemplate& tpl = templates_[static_cast<std::size_t>(currentIndex_)];
        if (row < 0 || !selected || !selected->localIndex
            || *selected->localIndex >= tpl.values.size())
        {
            return;
        }

        const std::size_t localIndex = *selected->localIndex;
        const bool removedFrameStructure =
            sed::text::iequalsAscii(
                tpl.values[localIndex].name, "frame")
            || sed::text::iequalsAscii(
                tpl.values[localIndex].name, "numframes");
        if (selected->inheritedValue && !selected->repeatable)
        {
            const std::string name = selected->value.name;
            tpl.values.erase(
                std::remove_if(tpl.values.begin(), tpl.values.end(), [&name](const TplValue& value)
                    {
                        return sed::text::iequalsAscii(value.name, name);
                    }),
                tpl.values.end());
        }
        else
        {
            tpl.values.erase(tpl.values.begin()
                + static_cast<std::vector<TplValue>::difference_type>(localIndex));
        }
        if (removedFrameStructure)
        {
            sed::level::thingSyncNumFrames(tpl);
        }
        loadCurrentTemplate();
    }

    void fitPreviewBounds(const ThingBox& previewBox, const bool resetOrbit)
    {
        if (!previewView_ || !previewStack_ || previewStack_->currentWidget() != previewView_)
        {
            return;
        }

        QVector3D minv(
            static_cast<float>(std::min(previewBox.x1, previewBox.x2)),
            static_cast<float>(std::min(previewBox.y1, previewBox.y2)),
            static_cast<float>(std::min(previewBox.z1, previewBox.z2)));
        QVector3D maxv(
            static_cast<float>(std::max(previewBox.x1, previewBox.x2)),
            static_cast<float>(std::max(previewBox.y1, previewBox.y2)),
            static_cast<float>(std::max(previewBox.z1, previewBox.z2)));
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
            previewView_->setOrbitViewImmediate(
                kAssetPreviewDefaultYawDeg,
                kAssetPreviewDefaultPitchDeg,
                kAssetPreviewDefaultRollDeg);
        }
        previewView_->frameWorldBounds(minv, maxv, false);
        previewView_->tightenOrthoClipToWorldBounds(minv, maxv);
        QTimer::singleShot(0, previewView_, [view = previewView_, minv, maxv, resetOrbit]()
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

    void fitPreviewView(const bool resetOrbit = true)
    {
        if (!previewView_ || !previewStack_ || previewStack_->currentWidget() != previewView_ || !hasCurrentTemplate())
        {
            return;
        }
        if (previewCollisionOnlyFitBounds_)
        {
            fitPreviewBounds(*previewCollisionOnlyFitBounds_, resetOrbit);
            return;
        }

        const ThingTemplate& tpl = templates_[static_cast<std::size_t>(currentIndex_)];
        if (const std::optional<ThingBox> previewBox = inferTemplatePreviewBox(
                tpl,
                &previewTemplates_,
                tpl.name,
                previewResources_))
        {
            fitPreviewBounds(*previewBox, resetOrbit);
            return;
        }

        QTimer::singleShot(0, previewView_, [view = previewView_, resetOrbit]()
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
                    view->fitViewToLevel();
                }
            });
    }

    void updatePreviewSpritePixmap()
    {
        if (!previewSpriteLabel_)
        {
            return;
        }
        if (!previewSpriteImage_ || previewSpriteImage_->isNull())
        {
            previewSpriteLabel_->setPixmap(QPixmap{});
            return;
        }

        previewSpriteLabel_->setPixmap(
            makeTemplatePreviewPixmap(
                *previewSpriteImage_,
                previewSpriteLabel_->contentsRect().size()));
    }

    void repositionPreviewSpriteAnimControls()
    {
        if (!previewSpriteLabel_ || !previewSpriteAnimBtn_ || !previewSpriteAnimSpeedSlider_)
        {
            return;
        }

        const int margin = 8;
        const int buttonX = previewSpriteLabel_->width() - previewSpriteAnimBtn_->width() - margin;
        const int buttonY = previewSpriteLabel_->height() - previewSpriteAnimBtn_->height() - margin;
        previewSpriteAnimBtn_->move(buttonX, buttonY);
        const int sliderY = buttonY + std::max(0, (previewSpriteAnimBtn_->height() - previewSpriteAnimSpeedSlider_->height()) / 2);
        const int sliderX = buttonX - previewSpriteAnimSpeedSlider_->width() - 6;
        previewSpriteAnimSpeedSlider_->move(std::max(margin, sliderX), sliderY);
    }

    const assets::CmpPalette* previewPalette()
    {
        if (previewPaletteResolved_)
        {
            return previewPalette_ ? &*previewPalette_ : nullptr;
        }

        previewPaletteResolved_ = true;
        if (!previewResources_ || previewMasterCmp_.empty())
        {
            return nullptr;
        }

        try
        {
            if (const std::optional<std::vector<std::byte>> cmpBytes =
                    previewResources_->tryReadBinaryWithExt(previewMasterCmp_, ".cmp"))
            {
                previewPalette_ = assets::parseCmpPalette(*cmpBytes);
                return previewPalette_ ? &*previewPalette_ : nullptr;
            }
        }
        catch (const std::exception&)
        {
        }

        return nullptr;
    }

    void updatePreviewSpriteAnimationSpeed()
    {
        const int fps = previewSpriteAnimSpeedSlider_ ? std::max(1, previewSpriteAnimSpeedSlider_->value()) : 10;
        const int intervalMs = std::max(20, static_cast<int>(std::lround(1000.0 / static_cast<double>(fps))));
        if (previewSpriteAnimTimer_)
        {
            previewSpriteAnimTimer_->setInterval(intervalMs);
        }
        if (previewSpriteAnimSpeedSlider_)
        {
            previewSpriteAnimSpeedSlider_->setToolTip(QString("Animation speed: %1 fps").arg(fps));
        }
    }

    void loadPreviewSpriteAnimFrames(const std::string_view spriteName)
    {
        previewSpriteAnimFrames_.clear();
        previewSpriteAnimIndex_ = 0;
        if (!previewResources_)
        {
            return;
        }

        const std::shared_ptr<const assets::SprFile> sprite =
            ::sed::ui::detail::loadSharedPreviewSprite(previewResources_, spriteName);
        if (!sprite)
        {
            return;
        }

        try
        {
            const std::optional<std::vector<std::byte>> matBytes =
                previewResources_->tryReadBinaryWithExt(sprite->material, ".mat");
            if (!matBytes)
            {
                return;
            }

            assets::MatFile mat = assets::MatFile::parse(
                std::vector<std::byte>(matBytes->begin(), matBytes->end()),
                previewPalette());

            const std::int32_t count = mat.celCount();
            if (count <= 1)
            {
                return;
            }

            previewSpriteAnimFrames_.reserve(static_cast<std::size_t>(count));
            for (std::int32_t i = 0; i < count; ++i)
            {
                QImage img = ::sed::ui::detail::qImageFromRgba8(mat.decodeCelRgba8(i));
                if (img.isNull())
                {
                    previewSpriteAnimFrames_.clear();
                    return;
                }
                previewSpriteAnimFrames_.push_back(std::move(img));
            }
        }
        catch (const std::exception&)
        {
            previewSpriteAnimFrames_.clear();
        }
    }

    void startPreviewSpriteAnimation()
    {
        if (previewSpriteAnimFrames_.size() <= 1 || !previewSpriteAnimTimer_)
        {
            return;
        }
        updatePreviewSpriteAnimationSpeed();
        previewSpriteAnimTimer_->start();
        if (previewSpriteAnimBtn_)
        {
            previewSpriteAnimBtn_->setText(QStringLiteral("\u25A0"));
        }
    }

    void stopPreviewSpriteAnimation()
    {
        if (previewSpriteAnimTimer_)
        {
            previewSpriteAnimTimer_->stop();
        }
        previewSpriteAnimIndex_ = 0;
        if (!previewSpriteAnimFrames_.empty())
        {
            previewSpriteImage_ = previewSpriteAnimFrames_.front();
            updatePreviewSpritePixmap();
        }
        if (previewSpriteAnimBtn_)
        {
            previewSpriteAnimBtn_->setText(QStringLiteral("\u25B6"));
        }
    }

    void resetPreviewSpriteAnimationState()
    {
        if (previewSpriteAnimTimer_)
        {
            previewSpriteAnimTimer_->stop();
        }
        previewSpriteAnimFrames_.clear();
        previewSpriteAnimIndex_ = 0;
        if (previewSpriteAnimBtn_)
        {
            previewSpriteAnimBtn_->setText(QStringLiteral("\u25B6"));
            previewSpriteAnimBtn_->hide();
        }
        if (previewSpriteAnimSpeedSlider_)
        {
            previewSpriteAnimSpeedSlider_->hide();
        }
    }

    void setupPreviewSpriteAnimation(const std::string_view spriteName)
    {
        resetPreviewSpriteAnimationState();
        loadPreviewSpriteAnimFrames(spriteName);
        if (previewSpriteAnimFrames_.size() > 1 && previewSpriteAnimBtn_ && previewSpriteAnimSpeedSlider_)
        {
            previewSpriteAnimBtn_->show();
            previewSpriteAnimSpeedSlider_->show();
            repositionPreviewSpriteAnimControls();
            startPreviewSpriteAnimation();
        }
    }

    QIcon collisionPreviewIcon(const bool visible) const
    {
        const QIcon icon = ui::makeViewportIcon(ui::ViewportIcon::thingBoxes);
        return visible ? icon : ui::makeCrossedOutViewportIcon(icon);
    }

    void updateCollisionPreviewToggle(const bool visible, const bool spriteThumbnailMode = false)
    {
        if (!previewCollisionToggleButton_)
        {
            return;
        }

        previewCollisionToggleButton_->setVisible(visible);
        {
            const QSignalBlocker blocker(previewCollisionToggleButton_);
            previewCollisionToggleButton_->setChecked(previewShowCollision_);
        }
        previewCollisionToggleButton_->setIcon(collisionPreviewIcon(previewShowCollision_));
        if (spriteThumbnailMode)
        {
            previewCollisionToggleButton_->setToolTip(previewShowCollision_
                ? QStringLiteral("Show SPR thumbnail")
                : QStringLiteral("Show collision preview"));
        }
        else
        {
            previewCollisionToggleButton_->setToolTip(previewShowCollision_
                ? QStringLiteral("Hide collision and editor-bounds preview")
                : QStringLiteral("Show collision and editor-bounds preview"));
        }
    }

    void updateCollisionPreviewLegend(
        const QString& reason,
        const render::thing_overlay::CollisionShape collisionShape,
        const bool showCollisionShape,
        const bool showIjimDimensions,
        const bool showTemplateBBox = false,
        const bool showBroadPhaseTreeGuide = false)
    {
        collisionLegendPresentation_ = CollisionLegendPresentation{
            reason, collisionShape, showCollisionShape, showIjimDimensions,
            showTemplateBBox, showBroadPhaseTreeGuide};
        if (!previewCollisionLegend_)
        {
            return;
        }
        if (!previewShowCollision_)
        {
            previewCollisionLegend_->hide();
            return;
        }

        QString legend{};
        if (!reason.isEmpty())
        {
            legend = reason.toHtmlEscaped() + QLatin1Char(' ');
        }
        if (showCollisionShape)
        {
            legend += QStringLiteral("<span style='color:#32fa47'>Green</span>: ");
            if (collisionShape == render::thing_overlay::CollisionShape::sphereTree)
            {
                const QString factor = ui::formatUiNumber(
                    previewView_
                        ? previewView_->motsTreeCollisionPreviewMeshRadiusFactor()
                        : 0.75f,
                    'g',
                    4);
                legend += QStringLiteral(
                    "<b>Collision Tree</b> — one sphere per 3DO hierarchy mesh. Preview radius is the effective authored mesh radius × <b>%1</b>; MotS uses %2 by default, and this preview-only scale does not change collision parameters or exports. Preview uses the default hierarchy pose, not animated joints.")
                    .arg(factor.toHtmlEscaped(), ui::formatUiNumber(0.75, 'g', 4).toHtmlEscaped());
            }
            else if (collisionShape == render::thing_overlay::CollisionShape::sphere)
            {
                legend += QStringLiteral(
                    "sphere from <b>collide</b> and <b>size</b>.");
            }
            else if (collisionShape == render::thing_overlay::CollisionShape::face)
            {
                legend += QStringLiteral("face collision from <b>collide</b>.");
            }
            else
            {
                legend += QStringLiteral("collision shape.");
            }
        }
        if (showBroadPhaseTreeGuide)
        {
            legend += QStringLiteral(
                " <span style='color:#eb57ff'>Magenta</span>: Broad-phase sphere centered on the Thing from <b>treesize</b>. It is separate from the green per-mesh narrow-phase spheres.");
        }
        if (showIjimDimensions)
        {
            legend += QStringLiteral(
                " <span style='color:#38d1ff'>Cyan</span>: IJIM authored bounding box guide from <b>collwidth</b> and <b>collheight</b>; not active collision.");
        }
        if (showTemplateBBox)
        {
            legend += QStringLiteral(
                " <span style='color:#ffd166'>BBOX</span>: authored template editor bounds, shown because resource bounds are unavailable.");
        }
        previewCollisionLegend_->setText(legend);
        previewCollisionLegend_->show();
    }

    bool showCollisionOnlyPreview(
        Thing previewThing,
        const QString& reason,
        const bool refit)
    {
        const render::thing_overlay::CollisionDescription collision =
            render::thing_overlay::resolveCollisionDescription(
                previewLevel_, &previewTemplates_, previewThing);
        const bool spherePreview = collision.hasRadius
            && collision.shape == render::thing_overlay::CollisionShape::sphere;
        const bool broadPhaseTreePreview = collision.broadPhaseRadius.has_value()
            && collision.shape == render::thing_overlay::CollisionShape::sphereTree;
        const auto authoredGuide =
            render::thing_overlay::resolveIjimAuthoredCollisionBoundsGuide(
                previewLevel_, &previewTemplates_, previewThing);
        const ThingBox templateBounds = render::resolveThingRenderBox(
            previewLevel_, &previewTemplates_, previewThing);
        // Authored BBOX is a fallback when no usable collision visualization
        // exists. Do not let it expand framing for a real sphere/guide, since
        // that can make tiny effective collision sizes nearly invisible.
        const bool templateBBoxPreview = previewShowCollision_
            && !spherePreview && !broadPhaseTreePreview && !authoredGuide && !templateBounds.IsZero();
        if (!spherePreview && !broadPhaseTreePreview && !authoredGuide && !templateBBoxPreview)
        {
            return false;
        }

        // Fit the viewport to the effective preview geometry. A unit-size
        // fallback made small inherited `size` values (such as 0.04) look tiny.
        constexpr double kMinimumPreviewHalfExtent = 0.05;
        const double broadPhaseRadius = broadPhaseTreePreview
            ? *collision.broadPhaseRadius
            : 0.0;
        double halfWidth = std::max(
            spherePreview ? collision.radius : 0.0,
            broadPhaseRadius);
        double halfHeight = std::max(
            spherePreview ? collision.radius : 0.0,
            broadPhaseRadius);
        if (authoredGuide)
        {
            halfWidth = std::max(halfWidth, authoredGuide->halfWidth);
            halfHeight = std::max(halfHeight, authoredGuide->halfHeight);
        }
        // Fit small authored collision dimensions at their real scale instead
        // of inflating them to a generic minimum. Use the fallback only when
        // neither an effective shape nor a complete authored guide has extent.
        if (!(halfWidth > 0.0))
        {
            halfWidth = kMinimumPreviewHalfExtent;
        }
        if (!(halfHeight > 0.0))
        {
            halfHeight = kMinimumPreviewHalfExtent;
        }
        ThingBox fitBounds{};
        if (templateBBoxPreview)
        {
            previewThing.bbox = templateBounds;
            fitBounds.Set(
                std::min(-halfWidth, std::min(templateBounds.x1, templateBounds.x2)),
                std::min(-halfWidth, std::min(templateBounds.y1, templateBounds.y2)),
                std::min(-halfHeight, std::min(templateBounds.z1, templateBounds.z2)),
                std::max(halfWidth, std::max(templateBounds.x1, templateBounds.x2)),
                std::max(halfWidth, std::max(templateBounds.y1, templateBounds.y2)),
                std::max(halfHeight, std::max(templateBounds.z1, templateBounds.z2)));
        }
        else
        {
            previewThing.bbox.Set(
                -halfWidth, -halfWidth, -halfHeight,
                halfWidth, halfWidth, halfHeight);
            fitBounds = previewThing.bbox;
        }
        previewCollisionOnlyFitBounds_ = fitBounds;
        previewLevel_.things = { std::move(previewThing) };
        const bool shouldFit = refit || previewStack_->currentWidget() != previewView_;

        previewView_->setResourceLocator(previewResources_);
        previewView_->setTemplateDatabase(&previewTemplates_);
        if (refit)
        {
            previewView_->setRenderPipeline(GlView::RenderPipeline::Editor);
            previewView_->resetPreviewViewportState();
        }
        previewView_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
        previewView_->setShowGrid(false);
        previewView_->setShowSolid(true);
        previewView_->setShowTextures(true);
        previewView_->setShowThings(true);
        previewView_->setShowEditorCubes(false);
        previewView_->setShowPreviewCubes(false);
        previewView_->setShowThingGizmos(false);
        previewView_->setThingViewMode(templateBBoxPreview
            ? GlView::ThingViewMode::Boxes
            : GlView::ThingViewMode::Shaded);
        previewView_->setShowThingCollisionShapes(previewShowCollision_);
        previewView_->setShowTemplateCollisionBoundsGuide(
            previewShowCollision_ && projectType_ == ProjectType::IJIM);
        previewView_->setViewportPickingEnabled(false);
        previewView_->setHoverPickEnabled(false);
        previewView_->setLevel(
            &previewLevel_,
            shouldFit ? GlView::SetLevelMode::resetView : GlView::SetLevelMode::keepView);
        updateCollisionPreviewToggle(true);
        updateCollisionPreviewLegend(
            reason,
            collision.shape,
            spherePreview,
            projectType_ == ProjectType::IJIM && authoredGuide.has_value(),
            templateBBoxPreview,
            broadPhaseTreePreview);
        previewStack_->setCurrentWidget(previewView_);
        if (shouldFit)
        {
            fitPreviewBounds(fitBounds, true);
        }
        return true;
    }

    void refreshPreview(const bool refit)
    {
        if (!previewStack_ || !previewEmptyLabel_ || !previewView_)
        {
            return;
        }

        previewCollisionOnlyFitBounds_.reset();
        // The preview view keeps a non-owning pointer to both previewLevel_
        // and previewTemplates_. Detach it before either backing object is
        // cleared or rebuilt so repaint/resource callbacks cannot observe
        // invalidated template storage during an inline parameter commit.
        previewView_->setLevel(nullptr, GlView::SetLevelMode::keepView);
        previewView_->setTemplateDatabase(nullptr);
        if (previewCollisionLegend_)
        {
            previewCollisionLegend_->hide();
        }
        if (previewCollisionToggleButton_)
        {
            previewCollisionToggleButton_->hide();
        }
        previewView_->setShowThingCollisionShapes(false);
        previewView_->setShowTemplateCollisionBoundsGuide(false);
        previewEmptyLabel_->setText("No previewable 3DO model or sprite in this template.");

        if (!hasCurrentTemplate())
        {
            previewTemplates_.clear();
            previewStack_->setCurrentWidget(previewEmptyLabel_);
            return;
        }

        const std::size_t current = static_cast<std::size_t>(currentIndex_);
        const ThingTemplate& currentTemplate = templates_[current];
        std::string previewTemplateName = ::sed::text::trim(currentTemplate.name);
        if (previewTemplateName.empty())
        {
            previewTemplateName = "__template_preview__";
        }
        if (previewCollisionToggleTemplateName_ != previewTemplateName)
        {
            previewCollisionToggleTemplateName_ = previewTemplateName;
            previewShowCollision_ = true;
            if (previewCollisionToggleButton_)
            {
                const QSignalBlocker blocker(previewCollisionToggleButton_);
                previewCollisionToggleButton_->setChecked(true);
            }
        }

        previewTemplates_.clear();
        for (std::size_t i = 0; i < templates_.size(); ++i)
        {
            ThingTemplate tpl = templates_[i];
            std::string name = ::sed::text::trim(tpl.name);
            if (name.empty())
            {
                if (i != current)
                {
                    continue;
                }
                tpl.name = previewTemplateName;
            }

            try
            {
                (void)previewTemplates_.addOrUpdate(std::move(tpl), true);
            }
            catch (const std::exception&)
            {
                // Keep preview resilient while the user is editing invalid/incomplete templates.
            }
        }

        previewLevel_.clear();
        previewLevel_.kind = projectType_;
        previewLevel_.masterCMP = previewMasterCmp_;
        previewLevel_.ppunit = 320.0;
        previewLevel_.templates.clear();
        for (std::size_t i = 0; i < previewTemplates_.size(); ++i)
        {
            if (const ThingTemplate* tpl = previewTemplates_.get(i))
            {
                previewLevel_.templates.push_back(*tpl);
            }
        }
        previewLevel_.sectors.push_back(Sector{});
        previewLevel_.sectors.back().layer = 0;
        previewLevel_.sectors.back().ambient = ColorWhite;

        Thing previewThing{};
        previewThing.name = previewTemplateName;
        previewThing.templateName = previewTemplateName;
        previewThing.sector = 0;
        previewThing.x = 0.0;
        previewThing.y = 0.0;
        previewThing.z = 0.0;
        previewThing.pch = 0.0;
        previewThing.yaw = 0.0;
        previewThing.rol = 0.0;
        const std::optional<std::string> spriteName = resolvedTemplatePreviewField(
            currentTemplate, &previewTemplates_, previewTemplateName, "sprite");
        std::optional<std::string> modelName = resolvedTemplatePreviewField(
            currentTemplate, &previewTemplates_, previewTemplateName, "model3d");
        if (!modelName)
        {
            modelName = resolvedTemplatePreviewField(
                currentTemplate, &previewTemplates_, previewTemplateName, "model3do");
        }

        if (!spriteName && !modelName)
        {
            if (showCollisionOnlyPreview(
                    previewThing,
                    QStringLiteral("No model or sprite asset."),
                    refit))
            {
                return;
            }
            previewView_->setTemplateDatabase(nullptr);
            previewView_->setLevel(nullptr, GlView::SetLevelMode::keepView);
            previewEmptyLabel_->setText("No previewable 3DO model or sprite in this template.");
            previewStack_->setCurrentWidget(previewEmptyLabel_);
            return;
        }

        previewSpriteImage_ = {};
        resetPreviewSpriteAnimationState();
        previewSpriteLabel_->setPixmap(QPixmap{});

        if (spriteName)
        {
            previewSpriteImage_ = loadTemplateSpritePreviewImage(previewResources_, previewMasterCmp_, *spriteName);
            if (previewSpriteImage_ && !previewSpriteImage_->isNull())
            {
                previewView_->setTemplateDatabase(nullptr);
                previewView_->setLevel(nullptr, GlView::SetLevelMode::keepView);
                updatePreviewSpritePixmap();
                setupPreviewSpriteAnimation(*spriteName);
                const render::thing_overlay::CollisionDescription spriteCollision =
                    render::thing_overlay::resolveCollisionDescription(
                        previewLevel_, &previewTemplates_, previewThing);
                const bool spriteHasSpherePreview = spriteCollision.hasRadius
                    && spriteCollision.shape == render::thing_overlay::CollisionShape::sphere;
                const bool spriteHasBoundsGuide =
                    render::thing_overlay::resolveIjimAuthoredCollisionBoundsGuide(
                        previewLevel_, &previewTemplates_, previewThing).has_value();
                const bool spriteHasTreeCollision =
                    spriteCollision.shape == render::thing_overlay::CollisionShape::sphereTree;
                const bool hasSpriteCollisionPreview =
                    spriteHasSpherePreview || spriteHasBoundsGuide
                    || spriteCollision.broadPhaseRadius.has_value();
                // Collision-tree spheres belong to the model hierarchy, so
                // continue to the 3DO preview when both resources exist.
                if (!spriteHasTreeCollision || !modelName)
                {
                    updateCollisionPreviewToggle(hasSpriteCollisionPreview, true);
                    if (hasSpriteCollisionPreview && previewShowCollision_)
                    {
                        if (showCollisionOnlyPreview(
                                previewThing,
                                spriteHasTreeCollision
                                    ? QStringLiteral("SPR preview; only the treesize broad-phase guide is available.")
                                    : QStringLiteral("SPR collision preview."),
                                refit))
                        {
                            return;
                        }
                    }
                    if (spriteHasTreeCollision && !modelName)
                    {
                        updateCollisionPreviewLegend(
                            QStringLiteral("MotS Collision Tree preview needs a loaded 3DO model."),
                            spriteCollision.shape,
                            false,
                            false,
                            false,
                            spriteCollision.broadPhaseRadius.has_value());
                    }
                    else if (previewCollisionLegend_)
                    {
                        previewCollisionLegend_->hide();
                    }
                    previewStack_->setCurrentWidget(previewSpriteLabel_);
                    return;
                }
            }

            if (!modelName)
            {
                const QString message = QString("Sprite unavailable: %1.")
                    .arg(QString::fromStdString(*spriteName));
                if (showCollisionOnlyPreview(previewThing, message, refit))
                {
                    return;
                }
                previewView_->setTemplateDatabase(nullptr);
                previewView_->setLevel(nullptr, GlView::SetLevelMode::keepView);
                previewEmptyLabel_->setText(QString("Couldn't preview sprite %1.").arg(QString::fromStdString(*spriteName)));
                previewStack_->setCurrentWidget(previewEmptyLabel_);
                return;
            }
        }

        const int previewMode = previewRenderCombo_ ? previewRenderCombo_->currentData().toInt() : 1;
        ui::detail::applyThingAssetPreviewRenderMode(*previewView_, previewMode);
        const std::optional<ThingBox> modelBox =
            previewResources_ && modelName
                ? ui::detail::loadSharedPreviewModelBox(previewResources_, *modelName)
                : std::nullopt;
        if (!modelBox)
        {
            const QString message = QString("Model unavailable: %1.")
                .arg(modelName ? QString::fromStdString(*modelName) : QString{});
            if (showCollisionOnlyPreview(previewThing, message, refit))
            {
                return;
            }
            previewView_->setTemplateDatabase(nullptr);
            previewView_->setLevel(nullptr, GlView::SetLevelMode::keepView);
            previewEmptyLabel_->setText(QString("Couldn't preview model %1.")
                    .arg(modelName ? QString::fromStdString(*modelName) : QString{}));
            previewStack_->setCurrentWidget(previewEmptyLabel_);
            return;
        }
        previewThing.bbox = *modelBox;
        previewLevel_.things.push_back(previewThing);

        previewView_->setResourceLocator(previewResources_);
        ui::detail::primeSharedModelPreviewAssets(
            *previewView_, previewResources_, previewMasterCmp_, *modelName);
        previewView_->setTemplateDatabase(&previewTemplates_);
        if (refit)
        {
            previewView_->setRenderPipeline(GlView::RenderPipeline::Editor);
            previewView_->resetPreviewViewportState();
        }
        previewView_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
        previewView_->setShowGrid(false);
        const render::thing_overlay::CollisionDescription modelCollision =
            render::thing_overlay::resolveCollisionDescription(
                previewLevel_, &previewTemplates_, previewThing);
        const bool modelSphereCollisionPreview = modelCollision.hasRadius
            && modelCollision.shape == render::thing_overlay::CollisionShape::sphere;
        const bool modelTreeCollisionPreview =
            modelCollision.shape == render::thing_overlay::CollisionShape::sphereTree;
        const bool modelFaceCollisionPreview =
            modelCollision.shape == render::thing_overlay::CollisionShape::face
            && modelBox.has_value();
        const bool modelHasIjimBoundsGuide =
            render::thing_overlay::resolveIjimAuthoredCollisionBoundsGuide(
                previewLevel_, &previewTemplates_, previewThing).has_value();
        const bool modelHasBroadPhaseTreeGuide = modelCollision.broadPhaseRadius.has_value();
        const bool modelHasCollisionPreview = modelSphereCollisionPreview
            || modelTreeCollisionPreview
            || modelFaceCollisionPreview
            || modelHasIjimBoundsGuide
            || modelHasBroadPhaseTreeGuide;
        previewView_->setShowThingCollisionShapes(previewShowCollision_);
        previewView_->setShowTemplateCollisionBoundsGuide(
            previewShowCollision_ && projectType_ == ProjectType::IJIM);
        ui::detail::applyThingAssetPreviewRenderMode(*previewView_, previewMode);
        previewView_->setShowThings(true);
        previewView_->setShowEditorCubes(false);
        previewView_->setShowPreviewCubes(false);
        previewView_->setShowThingGizmos(false);
        previewView_->setLightingMode(GlView::LightingMode::Gouraud);
        previewView_->setCameraProjectionType(
            previewProjectionCombo_
                ? static_cast<app::ViewportCamera::ProjectionType>(previewProjectionCombo_->currentData().toInt())
                : app::ViewportCamera::ProjectionType::Orthographic);
        previewView_->setViewportPickingEnabled(false);
        previewView_->setHoverPickEnabled(false);
        previewView_->setLevel(&previewLevel_, refit ? GlView::SetLevelMode::resetView : GlView::SetLevelMode::keepView);
        ui::detail::applyThingAssetPreviewRenderMode(*previewView_, previewMode);
        updateCollisionPreviewToggle(modelHasCollisionPreview);
        if (modelHasCollisionPreview)
        {
            updateCollisionPreviewLegend(
                QString{},
                modelCollision.shape,
                modelSphereCollisionPreview || modelTreeCollisionPreview || modelFaceCollisionPreview,
                projectType_ == ProjectType::IJIM && modelHasIjimBoundsGuide,
                false,
                modelHasBroadPhaseTreeGuide);
        }
        else if (previewCollisionLegend_)
        {
            previewCollisionLegend_->hide();
        }
        previewStack_->setCurrentWidget(previewView_);

        if (refit)
        {
            fitPreviewView();
        }
    }

    std::vector<ThingTemplate> templates_{};
    std::function<std::optional<QString>(AdjType, const QString&, const QString&, const std::vector<ThingTemplate>*)> browseResource_{};
    ProjectType projectType_{ ProjectType::JKDF2 };
    std::string previewMasterCmp_{};
    std::optional<assets::ResourceLocator> previewResourcesStorage_{};
    const assets::ResourceLocator* previewResources_{};
    std::shared_ptr<const assets::ResourceLocator> previewThumbnailResources_{};
    bool archiveReloadWarningShown_{};
    assets::Templates previewTemplates_{};
    assets::Templates templateListPreviewDb_{};
    std::optional<::sed::assets::JlhTree> templateCategoryTree_{};
    TemplateCategoryEditState pendingTemplateCategoryEditState_{ TemplateCategoryEditState::none };
    QString pendingTemplateCategoryEditPath_{};
    QString pendingTemplateCategoryEditParentPath_{};
    QString pendingTemplateCategoryEditOriginalLabel_{};
    Level previewLevel_{};
    QLineEdit* searchEdit_{};
    QComboBox* templateSortCombo_{};
    QTreeWidget* categoryTree_{};
    QListWidget* templateList_{};
    QWidget* detailPanel_{};
    QStackedWidget* previewStack_{};
    QLabel* previewEmptyLabel_{};
    QLabel* previewCollisionLegend_{};
    struct CollisionLegendPresentation
    {
        QString reason{};
        render::thing_overlay::CollisionShape shape{};
        bool showShape{};
        bool showIjimDimensions{};
        bool showTemplateBBox{};
        bool showBroadPhaseTreeGuide{};
    };
    std::optional<CollisionLegendPresentation> collisionLegendPresentation_{};
    QLabel* previewSpriteLabel_{};
    QToolButton* previewCollisionToggleButton_{};
    std::string previewCollisionToggleTemplateName_{};
    bool previewShowCollision_{ true };
    std::optional<ThingBox> previewCollisionOnlyFitBounds_{};
    QToolButton* previewSpriteAnimBtn_{};
    QSlider* previewSpriteAnimSpeedSlider_{};
    GlView* previewView_{};
    QTimer* previewSpriteAnimTimer_{};
    QTimer* previewSpinTimer_{};
    QElapsedTimer previewSpinElapsed_{};
    QTimer* previewRefitTimer_{};
    QTimer* templateIconLoadTimer_{};
    QTimer* templateIconKeepaliveTimer_{};
    std::shared_ptr<::sed::ui::detail::GlViewModelThumbnailRenderer> modelThumbnailRenderer_{};
    ::sed::ui::detail::PreviewResourceDiskWatcher* previewResourceDiskWatcher_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    int archiveReloadAttemptCountForTest_{};
    int previewResourceInvalidationCountForTest_{};
#endif
    bool previewAutoRotateEnabled_{ true };
    float previewAutoRotateSpeedDegPerSec_{ 72.0f };
    QComboBox* previewProjectionCombo_{};
    QComboBox* previewRenderCombo_{};
    QLineEdit* nameEdit_{};
    QLineEdit* parentEdit_{};
    QLineEdit* descEdit_{};
    QComboBox* typeBox_{};
    std::array<ui::ScrubFloatEdit*, 6> bboxSpins_{};
    QLineEdit* fieldSearchEdit_{};
    QTableWidget* fieldsTable_{};
    QPushButton* editFieldButton_{};
    QPushButton* removeFieldButton_{};
    std::vector<ui::EffectiveTemplateParameter> effectiveFieldRows_{};
    std::uint64_t inlineTemplateRefreshGeneration_{};
    std::optional<QImage> previewSpriteImage_{};
    std::vector<QImage> previewSpriteAnimFrames_{};
    int previewSpriteAnimIndex_{};
    std::optional<assets::CmpPalette> previewPalette_{};
    bool previewPaletteResolved_{};
    std::unordered_map<std::string, QIcon> previewIconCache_{};
    ::sed::ui::detail::PreviewThumbLoadQueue templateIconQueue_{};
    ::sed::ui::detail::PreviewThumbnailPublicationPacer
        templateIconPublicationPacer_{};
    int templateIconActiveJobs_{};
    int templateIconLoadSuspendDepth_{};
    bool templateIconLoadInProgress_{ false };
    std::unordered_set<std::string> templateIconPrepInFlight_{};
    std::unordered_set<std::string> templateIconInFlightKeys_{};
    bool loading_{};
    int currentIndex_{-1};
};


} // namespace

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
bool templateEditorPaletteAvailableForTest(const assets::ResourceLocator& resources, const std::string& cmp)
{
    TemplateEditorDialog dialog({}, QStringLiteral("test.tpl"), {}, ProjectType::JKDF2, cmp, &resources);
    return dialog.previewPaletteAvailableForTest();
}

std::vector<ThingTemplate> templateEditorTemplatesForTest(QDialog* dialog)
{
    const auto* editor = dynamic_cast<TemplateEditorDialog*>(dialog);
    return editor ? editor->templates() : std::vector<ThingTemplate>{};
}

bool templateEditorArchiveReloadRetryForTest(
    const assets::ResourceLocator& resources,
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
    TemplateEditorDialog dialog(
        {},
        QStringLiteral("test.tpl"),
        {},
        ProjectType::IJIM,
        {},
        &resources);
    auto* watcher = dialog.previewResourceDiskWatcherForTest();
    if ( !watcher )
    {
        return false;
    }
    watcher->syncNow();
    corruptArchive();
    watcher->notifyArchivePathChangedForTest();
    watcher->flushPendingChangeForTest();
    const std::optional<std::string> oldAfterFailure =
        dialog.previewResourceTextForTest(oldEntry);
    oldEntryVisibleAfterFailure = oldAfterFailure.has_value();
    if ( oldAfterFailure )
    {
        oldEntryPayloadAfterFailure = *oldAfterFailure;
    }
    reloadFailureObserved = dialog.archiveReloadWarningShownForTest();
    repairArchive();
    watcher->flushPendingChangeForTest();
    const std::optional<std::string> oldAfterRetry =
        dialog.previewResourceTextForTest(oldEntry);
    const std::optional<std::string> newAfterRetry =
        dialog.previewResourceTextForTest(newEntry);
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

namespace
{

QString normalizeTemplateArchiveWatchPathForTest(const QString& path)
{
    return QDir::cleanPath(
        QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()))
        .toCaseFolded();
}

bool templateArchivePathIsWatchedForTest(
    const ::sed::ui::detail::PreviewResourceDiskWatcher* watcher,
    const std::filesystem::path& archivePath)
{
    if ( !watcher )
    {
        return false;
    }
#ifdef _WIN32
    const QString expected = normalizeTemplateArchiveWatchPathForTest(
        QString::fromStdWString(archivePath.wstring()));
#else
    const QString expected = normalizeTemplateArchiveWatchPathForTest(
        QString::fromStdString(archivePath.string()));
#endif
    const QStringList watchedFiles = watcher->watchedFiles();
    return std::any_of(
        watchedFiles.cbegin(),
        watchedFiles.cend(),
        [&expected](const QString& watched)
        {
            return normalizeTemplateArchiveWatchPathForTest(watched) == expected;
        });
}

bool drainTemplateEditorPreviewWorkForTest(TemplateEditorDialog& dialog)
{
    constexpr int kMaxDrainMilliseconds = 5000;
    QElapsedTimer elapsed{};
    elapsed.start();
    while ( elapsed.elapsed() < kMaxDrainMilliseconds )
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        ::sed::ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if ( dialog.templateIconPendingCountForTest() == 0
            && dialog.templateIconActiveJobsForTest() == 0
            && ::sed::ui::detail::previewWorkerPool().activeThreadCount() == 0
            && dialog.templateListRowStateForTest(0)
                == static_cast<int>(::sed::ui::detail::PreviewThumbState::ready) )
        {
            return true;
        }

        const int remaining = kMaxDrainMilliseconds
            - static_cast<int>(elapsed.elapsed());
        if ( remaining <= 0 )
        {
            break;
        }
        QEventLoop waitLoop{};
        QTimer::singleShot(
            std::min(remaining, 8),
            &waitLoop,
            &QEventLoop::quit);
        waitLoop.exec(QEventLoop::AllEvents);
    }
    return false;
}

bool imageHasForegroundTintForTest(const QImage& image, const bool red)
{
    if ( image.isNull() )
    {
        return false;
    }

    for ( int y = 0; y < image.height(); ++y )
    {
        for ( int x = 0; x < image.width(); ++x )
        {
            const QColor pixel = image.pixelColor(x, y);
            if ( red
                ? pixel.red() >= 128
                    && pixel.red() > pixel.green() * 2
                    && pixel.red() > pixel.blue() * 2
                : pixel.green() >= 128
                    && pixel.green() > pixel.red() * 2
                    && pixel.green() > pixel.blue() * 2 )
            {
                return true;
            }
        }
    }
    return false;
}

bool imagesDifferForTest(const QImage& left, const QImage& right)
{
    if ( left.isNull() || right.isNull() )
    {
        return false;
    }
    if ( left.size() != right.size() || left.format() != right.format() )
    {
        return true;
    }
    for ( int y = 0; y < left.height(); ++y )
    {
        for ( int x = 0; x < left.width(); ++x )
        {
            if ( left.pixel(x, y) != right.pixel(x, y) )
            {
                return true;
            }
        }
    }
    return false;
}

} // namespace

app_tests::TemplateArchiveRefreshTestResult
templateEditorNestedArchiveRefreshForTest(
    std::vector<ThingTemplate> templates,
    const assets::ResourceLocator& resources,
    std::string previewMasterCmp,
    const std::string& modelName,
    const std::filesystem::path& archivePath,
    const std::function<void()>& writeBrokenArchive,
    const std::function<void()>& writeReplacementArchive)
{
    app_tests::TemplateArchiveRefreshTestResult result{};
    int nestedArchiveSignalCount = 0;
    bool ownerWasAliveForArchiveSignal = false;
    TemplateEditorDialog dialog(
        std::move(templates),
        QStringLiteral("template-archive-refresh.tpl"),
        {},
        ProjectType::IJIM,
        std::move(previewMasterCmp),
        &resources);
    dialog.setAttribute(Qt::WA_DontShowOnScreen);

    auto* outerWatcher = dialog.previewResourceDiskWatcherForTest();
    auto* nestedWatcher = dialog.previewViewResourceDiskWatcherForTest();
    result.outerWatcherReady = outerWatcher != nullptr;
    result.nestedWatcherReady = nestedWatcher != nullptr;
    if ( !outerWatcher || !nestedWatcher )
    {
        return result;
    }

    GlView* previewView = dialog.findChild<GlView*>(
        QStringLiteral("templatePreviewView"));
    if ( !previewView )
    {
        return result;
    }

    const QPointer<QObject> ownerGuard(&dialog);
    const QMetaObject::Connection archiveSignalConnection = QObject::connect(
        previewView,
        &GlView::resourceArchivesChanged,
        &dialog,
        [ownerGuard, &nestedArchiveSignalCount, &ownerWasAliveForArchiveSignal]()
        {
            if ( ownerGuard )
            {
                ownerWasAliveForArchiveSignal = true;
                ++nestedArchiveSignalCount;
            }
        });

    dialog.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    result.initialDrainCompleted =
        drainTemplateEditorPreviewWorkForTest(dialog);

    result.rowsBefore = dialog.templateListRowsForTest();
    result.iconBefore = dialog.templateListIconForTest(0);
    result.initialIconHasRedForeground = imageHasForegroundTintForTest(
        result.iconBefore,
        true);
    result.initialPendingRowCount = dialog.templateIconPendingCountForTest();
    result.initialActiveJobCount = dialog.templateIconActiveJobsForTest();
    result.initialPreviewWorkerPoolActiveCount =
        ::sed::ui::detail::previewWorkerPool().activeThreadCount();
    result.initialIconReady =
        dialog.templateListRowStateForTest(0)
        == static_cast<int>(::sed::ui::detail::PreviewThumbState::ready);
    result.iconCacheSizeBefore = dialog.previewIconCacheSizeForTest();
    result.payloadBefore = dialog.previewResourceTextForTest(modelName);
    result.ownerInvalidationCountBefore =
        dialog.previewResourceInvalidationCountForTest();

    result.outerArchiveWasWatched = templateArchivePathIsWatchedForTest(
        outerWatcher,
        archivePath);
    result.nestedArchiveWasWatched = templateArchivePathIsWatchedForTest(
        nestedWatcher,
        archivePath);

    // Disable OS archive notifications on both watchers. The test then drives
    // only the nested watcher seam, so an outer file event cannot accidentally
    // repair the owner list while this regression is running. The nested
    // watcher still executes its production callback, retry, and signal path.
    outerWatcher->setMountedArchiveWatchingEnabled(false);
    outerWatcher->syncNow();
    nestedWatcher->setMountedArchiveWatchingEnabled(false);
    nestedWatcher->syncNow();
    result.outerMountedArchiveWatchingDisabled =
        !outerWatcher->mountedArchiveWatchingEnabledForTest();
    result.nestedMountedArchiveWatchingDisabled =
        !nestedWatcher->mountedArchiveWatchingEnabledForTest();

    writeBrokenArchive();
    nestedWatcher->notifyArchivePathChangedForTest();
    nestedWatcher->flushPendingChangeForTest();
    result.payloadAfterFailure = dialog.previewResourceTextForTest(modelName);
    result.oldPayloadRetainedAfterFailure =
        result.payloadAfterFailure == result.payloadBefore;
    result.nestedRetryRetainedAfterFailure =
        nestedWatcher->archiveRetryPendingForTest();
    result.outerRetryPendingAfterNestedEvent =
        outerWatcher->archiveRetryPendingForTest();

    writeReplacementArchive();
    // Do not notify the watcher again. The retained timer event is the only
    // event that may publish the replacement archive.
    nestedWatcher->flushPendingChangeForTest();
    result.finalDrainCompleted =
        drainTemplateEditorPreviewWorkForTest(dialog);

    result.payloadAfterRepair = dialog.previewResourceTextForTest(modelName);
    result.rowsAfter = dialog.templateListRowsForTest();
    result.iconAfter = dialog.templateListIconForTest(0);
    result.finalIconHasGreenForeground = imageHasForegroundTintForTest(
        result.iconAfter,
        false);
    result.finalPendingRowCount = dialog.templateIconPendingCountForTest();
    result.finalActiveJobCount = dialog.templateIconActiveJobsForTest();
    result.finalPreviewWorkerPoolActiveCount =
        ::sed::ui::detail::previewWorkerPool().activeThreadCount();
    result.finalIconReady =
        dialog.templateListRowStateForTest(0)
        == static_cast<int>(::sed::ui::detail::PreviewThumbState::ready);
    result.iconCacheSizeAfter = dialog.previewIconCacheSizeForTest();
    result.ownerInvalidationCountAfter =
        dialog.previewResourceInvalidationCountForTest();
    result.nestedRetryClearedAfterRepair =
        !nestedWatcher->archiveRetryPendingForTest();
    result.nestedArchiveSignalCount = nestedArchiveSignalCount;
    result.nestedReloadAttemptCount =
        dialog.archiveReloadAttemptCountForTest();
    result.ownerWasAliveForArchiveSignal = ownerWasAliveForArchiveSignal;
    result.sourcePayloadChanged =
        result.payloadBefore.has_value()
        && result.payloadAfterRepair.has_value()
        && result.payloadBefore != result.payloadAfterRepair;
    result.oldPayloadAbsentAfterRepair =
        !result.payloadAfterRepair.has_value()
        || result.payloadAfterRepair != result.payloadBefore;
    result.replacementPayloadPublished = result.payloadAfterRepair.has_value();
    result.listIconChanged =
        imagesDifferForTest(result.iconBefore, result.iconAfter);
    QObject::disconnect(archiveSignalConnection);
    result.ready = true;
    return result;
}

QDialog* MainWindow::createTemplateEditorDialogForTest(
    std::vector<ThingTemplate> templates)
{
    return new TemplateEditorDialog(
        std::move(templates),
        QStringLiteral("test.tpl"),
        {},
        ProjectType::IJIM,
        {},
        nullptr,
        0.75f,
        this);
}

QDialog* MainWindow::createTemplateEditorDialogForTest(
    std::vector<ThingTemplate> templates,
    const ProjectType projectType,
    std::string previewMasterCmp,
    const assets::ResourceLocator& previewResources,
    const float motsTreeCollisionPreviewMeshRadiusFactor)
{
    return new TemplateEditorDialog(
        std::move(templates),
        QStringLiteral("test.tpl"),
        {},
        projectType,
        std::move(previewMasterCmp),
        &previewResources,
        motsTreeCollisionPreviewMeshRadiusFactor,
        this);
}
#endif

void MainWindow::showTemplateEditorDialog()
{
    const std::optional<QString> fileName = ui::templateFileNameForProject(document_.level().kind);
    if (!fileName)
    {
        QMessageBox::warning(this, "Template Editor", "Unsupported project type.");
        return;
    }

    std::filesystem::path tplPath{};
    if (const auto dataRootOpt = ui::findDataRoot())
    {
        tplPath = *dataRootOpt / fileName->toStdString();
    }
    else
    {
        tplPath = std::filesystem::current_path() / "data" / fileName->toStdString();
    }

    std::vector<ThingTemplate> projectTemplates{};

    try
    {
        std::error_code ec;
        if (std::filesystem::exists(tplPath, ec) && !ec)
        {
            const assets::Templates loaded = assets::loadTemplates(tplPath);
            projectTemplates.reserve(loaded.size());
            for (std::size_t i = 0; i < loaded.size(); ++i)
            {
                if (const ThingTemplate* tpl = loaded.get(i))
                {
                    projectTemplates.push_back(*tpl);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(
            this,
            "Template Editor",
            QString("Failed to load %1:\n%2")
                .arg(ui::nativeDisplayPath(tplPath), QString::fromUtf8(e.what())));
        return;
    }

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

    auto browseTemplateResource = [this, &projectTemplates, pickPreferredResourceRoot](
            const AdjType adjType,
            const QString& fieldName,
            const QString& currentValue,
            const std::vector<ThingTemplate>* templateOverrides)
        -> std::optional<QString>
    {
        const std::optional<ui::ResourcePickerKind> kind = ui::resourcePickerKindForAdjType(adjType);
        if (!kind)
        {
            return std::nullopt;
        }

        ui::ResourcePickerOptions options{};
        options.kind = *kind;
        options.initialName = currentValue.trimmed().toStdString();
        options.preferredDiskRoot = pickPreferredResourceRoot();
        if (!currentPath_.empty())
        {
            options.projectDirectory = currentPath_.parent_path();
        }
        options.title = QString("Choose %1").arg(fieldName).toStdString();
        options.previewCmp = document_.level().getMasterCMP();
        options.previewProjectType = document_.level().kind;
        options.templateDatabaseFallback = templates_.size() > 0 ? &templates_ : nullptr;
        options.templateOverrides = templateOverrides ? templateOverrides : &projectTemplates;

        const std::optional<std::string> picked = ui::pickResource(
            this, resourceLocatorSnapshot_, options);
        if (!picked)
        {
            return std::nullopt;
        }
        return QString::fromStdString(*picked);
    };

    TemplateEditorDialog dialog(
        projectTemplates,
        *fileName,
        browseTemplateResource,
        document_.level().kind,
        document_.level().getMasterCMP(),
        &resources_,
        glView_ ? glView_->motsTreeCollisionPreviewMeshRadiusFactor() : 0.75f,
        this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    assets::Templates database{};
    for (const ThingTemplate& tpl : dialog.templates())
    {
        database.addOrUpdate(tpl, true);
    }

    try
    {
        std::error_code ec;
        std::filesystem::create_directories(tplPath.parent_path(), ec);
        assets::saveTemplates(tplPath, database);
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(
            this,
            "Template Editor",
            QString("Failed to save %1:\n%2")
                .arg(ui::nativeDisplayPath(tplPath), QString::fromUtf8(e.what())));
        return;
    }

    std::unordered_map<std::string, ThingTemplate> byName{};
    for (const ThingTemplate& tpl : dialog.templates())
    {
        byName.emplace(::sed::text::upperAscii(tpl.name), tpl);
    }

    (void)document_.applyUndoable(
        "Templates: Level-local values synchronized",
        editor::DocumentDirty::Things | editor::DocumentDirty::Structure,
        [&byName](editor::DocumentState& state) -> bool
        {
            bool any = false;
            for (ThingTemplate& tpl : state.level.templates)
            {
                const auto it = byName.find(::sed::text::upperAscii(tpl.name));
                if (it == byName.end())
                {
                    continue;
                }

                if (!ui::templateEditorAuthoredValuesEqual(tpl, it->second))
                {
                    tpl = it->second;
                    any = true;
                }
            }
            return any;
        });

    loadGlobalTemplates();
    if (glView_)
    {
        glView_->notifyThingsChanged();
        glView_->setHighlightedSelection(document_.selection());
    }
    rebuildProperties();
    updateThingPropertiesPreview(false);
    updateEditActions();
    logMessage(MessageSeverity::info, QString("Saved templates: %1").arg(ui::nativeDisplayPath(tplPath)));
}

} // namespace sed
