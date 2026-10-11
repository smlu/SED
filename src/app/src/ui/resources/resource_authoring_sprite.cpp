#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/resources/resource_authoring_detail.hpp"
#include "ui/resources/resource_authoring_pane.hpp"

#include "gl_view.hpp"
#include "sed/assets/episode.hpp"
#include "sed/assets/cmp.hpp"
#include "sed/assets/mat.hpp"
#include "sed/assets/resource_locator.hpp"
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
#include "ui/components/preview_resource_revision_utils.hpp"
#include "ui/components/property_editor_controls.hpp"
#include "ui/resources/resource_picker.hpp"
#include "ui/resources/sprite_animation_clock.hpp"
#include "render/texture/texture_alpha_utils.hpp"
#include "render/overlays/thing_shape_overlay_utils.hpp"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QByteArray>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QCompleter>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QColor>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace sed::ui::resources::detail
{

namespace
{
constexpr auto kPreviewSpriteName = std::string_view{ "draft.spr" };
constexpr auto kPreviewTemplateName = std::string_view{ "__resource_authoring_preview__" };
constexpr std::size_t kSpritePreviewDecodedPixelBudget = 64u * 1024u * 1024u;
constexpr int kSpritePreviewMaximumCelCount = 256;

struct DecodedSpritePreviewCel
{
    QImage image{};
    std::int32_t alphaBpp{};
    QVector3D solidColor{ 1.0f, 1.0f, 1.0f };
    std::optional<ClassicSolidLightRamp> classicSolidLightRamp{};
};

class DoubleClickLineEdit final : public QLineEdit
{
public:
    explicit DoubleClickLineEdit(QWidget* parent = nullptr)
        : QLineEdit(parent)
    {
    }

    std::function<void()> doubleClickCallback{};

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        QLineEdit::mouseDoubleClickEvent(event);
        if ( doubleClickCallback )
        {
            doubleClickCallback();
        }
    }
};

class DoubleClickPreviewFilter final : public QObject
{
public:
    using QObject::QObject;

    std::function<void()> doubleClickCallback{};

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if ( event->type() == QEvent::MouseButtonDblClick )
        {
            const auto* const mouseEvent = static_cast<QMouseEvent*>(event);
            if ( mouseEvent->button() == Qt::LeftButton )
            {
                if ( doubleClickCallback )
                {
                    doubleClickCallback();
                }
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

std::string spritePreviewMaterialCacheKey(
    const assets::ResourceLocator* resources,
    const ProjectType projectType,
    const std::string_view masterCmp,
    const std::string_view material)
{
    std::string key(material);
    key += "|";
    key += ui::detail::previewResourceRevisionToken(resources, material, ".mat");
    key += "|";
    key += ui::detail::previewResourceRevisionToken(resources, masterCmp, ".cmp");
    key += "|";
    key += projectType == ProjectType::IJIM ? "IJIM" : "CLASSIC";
    return key;
}

std::vector<DecodedSpritePreviewCel> loadSpritePreviewCels(
    const assets::ResourceLocator* resources,
    const ProjectType projectType,
    const std::string_view masterCmp,
    const std::string_view material)
{
    std::vector<DecodedSpritePreviewCel> result{};
    if ( !resources || material.empty() || material == "-" )
    {
        return result;
    }

    try
    {
        std::optional<std::vector<std::byte>> matBytes =
            resources->tryReadBinaryWithExt(material, ".mat");
        if ( !matBytes )
        {
            return result;
        }

        std::optional<assets::CmpFile> cmp{};
        std::optional<assets::CmpPalette> palette{};
        if ( !masterCmp.empty() && masterCmp != "-" )
        {
            if ( const std::optional<std::vector<std::byte>> cmpBytes =
                     resources->tryReadBinaryWithExt(masterCmp, ".cmp") )
            {
                const std::span<const std::byte> bytes(cmpBytes->data(), cmpBytes->size());
                cmp = assets::parseCmp(bytes);
                palette = assets::parseCmpPalette(bytes);
            }
        }

        assets::MatFile mat = assets::MatFile::parse(
            std::move(*matBytes),
            palette ? &*palette : nullptr);
        if ( mat.celCount() <= 0 || mat.celCount() > kSpritePreviewMaximumCelCount )
        {
            return result;
        }

        std::size_t decodedPixelBytes = 0;
        result.reserve(static_cast<std::size_t>(mat.celCount()));
        const sed::ColorInfo& colorInfo = mat.colorInfo();
        const bool indexed = colorInfo.mode == sed::ColorMode::Indexed
            || colorInfo.mode == sed::ColorMode::IndexedCols;
        const std::int32_t headerAlphaBpp = colorInfo.alphaBPP <= 0
            ? 0
            : static_cast<std::int32_t>(colorInfo.alphaBPP);

        for ( int celIndex = 0; celIndex < mat.celCount(); ++celIndex )
        {
            const assets::MatFile::Cel& cel = mat.cel(celIndex);
            if ( cel.width <= 0
                || cel.height <= 0
                || static_cast<std::size_t>(cel.width)
                    > kSpritePreviewDecodedPixelBudget / 4u
                    / static_cast<std::size_t>(cel.height) )
            {
                return {};
            }
            const std::size_t expectedPixelBytes =
                static_cast<std::size_t>(cel.width)
                * static_cast<std::size_t>(cel.height)
                * 4u;
            if ( expectedPixelBytes > kSpritePreviewDecodedPixelBudget - decodedPixelBytes )
            {
                return {};
            }

            const sed::Rgba8Image rgba = mat.decodeCelRgba8(celIndex);
            if ( rgba.empty() )
            {
                return {};
            }
            if ( rgba.rgba.size() > kSpritePreviewDecodedPixelBudget - decodedPixelBytes )
            {
                return {};
            }
            decodedPixelBytes += rgba.rgba.size();

            QImage image(
                rgba.width,
                rgba.height,
                QImage::Format_RGBA8888);
            if ( image.isNull() )
            {
                return {};
            }
            const std::size_t rowBytes = static_cast<std::size_t>(rgba.width) * 4u;
            for ( int row = 0; row < rgba.height; ++row )
            {
                std::memcpy(
                    image.scanLine(row),
                    rgba.rgba.data() + static_cast<std::size_t>(row) * rowBytes,
                    rowBytes);
            }

            const std::int32_t inferredAlphaBpp = render::inferTextureAlphaBpp(image);
            std::int32_t alphaBpp = headerAlphaBpp;
            if ( (indexed || headerAlphaBpp == 1) && inferredAlphaBpp > 0 )
            {
                alphaBpp = inferredAlphaBpp;
            }
            else if ( alphaBpp == 0 )
            {
                alphaBpp = inferredAlphaBpp;
            }

            std::optional<ClassicSolidLightRamp> gpuRamp{};
            if ( projectType != ProjectType::IJIM && cmp )
            {
                const assets::CmpLightRamp ramp =
                    assets::makeCmpLightRamp(*cmp, cel.solidColorIndex);
                ClassicSolidLightRamp converted{};
                for ( std::size_t level = 0; level < ramp.size(); ++level )
                {
                    converted[level] = QVector3D(
                        static_cast<float>(ramp[level].r) / 255.0f,
                        static_cast<float>(ramp[level].g) / 255.0f,
                        static_cast<float>(ramp[level].b) / 255.0f);
                }
                gpuRamp = std::move(converted);
            }

            result.push_back(DecodedSpritePreviewCel{
                .image = std::move(image),
                .alphaBpp = alphaBpp,
                .solidColor = QVector3D(
                    static_cast<float>(cel.solidColor.r) / 255.0f,
                    static_cast<float>(cel.solidColor.g) / 255.0f,
                    static_cast<float>(cel.solidColor.b) / 255.0f),
                .classicSolidLightRamp = std::move(gpuRamp),
            });
        }
    }
    catch ( const std::exception& )
    {
        return {};
    }
    catch ( ... )
    {
        return {};
    }
    return result;
}

ThingBox spritePreviewBox(const assets::SprFile& spr, const ProjectType projectType)
{
    const double halfWidth = std::abs(static_cast<double>(spr.width)) * 0.5;
    const double halfHeight = std::abs(static_cast<double>(spr.height)) * 0.5;
    const bool ijimOriented = projectType == ProjectType::IJIM && spr.type == 2;

    ThingBox box{};
    if ( ijimOriented )
    {
        box.Set(
            -halfWidth, 0.0, -halfHeight,
            halfWidth, 0.0, halfHeight);
    }
    else if ( const std::optional<double> radius =
                  render::thing_overlay::spriteBillboardConservativeRadius(
                      spr.width, spr.height,
                      std::array<double, 3>{
                          spr.offsetXYZ[0], spr.offsetXYZ[1], spr.offsetXYZ[2] }) )
    {
        // Bounds draws this orbit-safe cube around the owning Thing. Fitting
        // a shifted, thin card instead clips that outline and recenters offsets.
        box.Set(-*radius, -*radius, -*radius, *radius, *radius, *radius);
    }
    else
    {
        // Zero dimensions are valid intermediate drafts. Preserve their
        // degenerate card rather than inventing nonzero authored dimensions.
        box.Set(
            spr.offsetXYZ[0] - halfWidth, spr.offsetXYZ[1], spr.offsetXYZ[2] - halfHeight,
            spr.offsetXYZ[0] + halfWidth, spr.offsetXYZ[1], spr.offsetXYZ[2] + halfHeight);
    }
    return box;
}

std::vector<std::byte> bytePayload(const std::string& text)
{
    std::vector<std::byte> bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(), [](const char value)
        {
            return static_cast<std::byte>(static_cast<unsigned char>(value));
        });
    return bytes;
}

template<std::size_t N>
QComboBox* makeIntegerCombo( QWidget* parent, const char* objectName, const std::array<std::pair<int, std::string_view>, N>& choices)
{
    auto* const combo = new QComboBox(parent);
    combo->setObjectName(QString::fromLatin1(objectName));
    for ( const auto& [value, label] : choices )
    {
        combo->addItem(QString::fromLatin1(label.data(), static_cast<qsizetype>(label.size())), value);
    }
    return combo;
}
} // namespace

void ResourceAuthoringDialog::buildSpriteEditor()
{
    auto* const splitter = new QSplitter(Qt::Horizontal, contentHost_);
    splitter->setObjectName(QStringLiteral("spr.editor_splitter"));

    auto* const formPanel = new QWidget(splitter);
    auto* const form = new QFormLayout(formPanel);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    auto* const materialRow = new QWidget(formPanel);
    auto* const materialLayout = new QHBoxLayout(materialRow);
    materialLayout->setContentsMargins(0, 0, 0, 0);
    auto* const materialEdit = new DoubleClickLineEdit(materialRow);
    spriteMaterial_ = materialEdit;
    spriteMaterial_->setObjectName(QStringLiteral("spr.material"));
    materialEdit->doubleClickCallback = [this]()
    {
        pickSpriteMaterial();
    };
    materialPickButton_ = new QToolButton(materialRow);
    materialPickButton_->setObjectName(QStringLiteral("spr.material.pick"));
    materialPickButton_->setText(QStringLiteral("…"));
    materialPickButton_->setAccessibleName(QStringLiteral("Choose sprite material"));
    materialPickButton_->setToolTip(QStringLiteral("Choose a MAT resource"));
    materialPickButton_->setAutoRaise(true);
    materialPickButton_->setFixedSize(28, 26);
    materialPickButton_->setFocusPolicy(Qt::NoFocus);
    materialLayout->addWidget(spriteMaterial_, 1);
    materialLayout->addWidget(materialPickButton_);
    form->addRow(QStringLiteral("Material"), materialRow);

    spriteTypeCombo_ = new QComboBox(formPanel);
    spriteTypeCombo_->setObjectName(QStringLiteral("spr.type"));
    form->addRow(QStringLiteral("Sprite type"), spriteTypeCombo_);

    spriteWidth_ = makeFloatControl(formPanel, "spr.width");
    spriteHeight_ = makeFloatControl(formPanel, "spr.height");
    spriteWidth_->setRange(0.0, static_cast<double>(std::numeric_limits<float>::max()));
    spriteHeight_->setRange(0.0, static_cast<double>(std::numeric_limits<float>::max()));
    form->addRow(QStringLiteral("Width"), spriteWidth_);
    form->addRow(QStringLiteral("Height"), spriteHeight_);

    static constexpr std::array geometryModes{
        std::pair{ 0, std::string_view{ "Not drawn" } },
        std::pair{ 1, std::string_view{ "Vertices" } },
        std::pair{ 2, std::string_view{ "Wireframe" } },
        std::pair{ 3, std::string_view{ "Solid" } },
        std::pair{ 4, std::string_view{ "Textured" } },
    };
    spriteGeometryMode_ = makeIntegerCombo(
        formPanel, "spr.geometry_mode", geometryModes);
    form->addRow(QStringLiteral("Geometry mode"), spriteGeometryMode_);

    spriteLightMode_ = new QComboBox(formPanel);
    spriteLightMode_->setObjectName(QStringLiteral("spr.light_mode"));
    form->addRow(QStringLiteral("Light mode"), spriteLightMode_);

    static constexpr std::array textureModes{
        std::pair{ 0, std::string_view{ "Affine (0)" } },
        std::pair{ 1, std::string_view{ "Perspective (1)" } },
        std::pair{ 2, std::string_view{ "Legacy / unknown (2)" } },
        std::pair{ 3, std::string_view{ "Legacy / unknown (3)" } },
    };
    spriteTextureMode_ = makeIntegerCombo(
        formPanel, "spr.texture_mode", textureModes);
    form->addRow(QStringLiteral("Texture mode"), spriteTextureMode_);

    spriteClassicExtraRow_ = new QWidget(formPanel);
    auto* const scalarLayout = new QHBoxLayout(spriteClassicExtraRow_);
    scalarLayout->setContentsMargins(0, 0, 0, 0);
    spriteExtraScalar_ = makeFloatControl(spriteClassicExtraRow_, "spr.extra_light.scalar");
    scalarLayout->addWidget(spriteExtraScalar_);
    form->addRow(QStringLiteral("Extra light"), spriteClassicExtraRow_);

    spriteIjimExtraRow_ = new QWidget(formPanel);
    auto* const rgbaLayout = new QHBoxLayout(spriteIjimExtraRow_);
    rgbaLayout->setContentsMargins(0, 0, 0, 0);
    extraLightSwatch_ = new QPushButton(spriteIjimExtraRow_);
    extraLightSwatch_->setObjectName(QStringLiteral("spr.extra_light.swatch"));
    ui::configureColorSwatchButton(extraLightSwatch_, displayExtraLightColor());
    rgbaLayout->addWidget(extraLightSwatch_, 1);
    form->addRow(QStringLiteral("Extra light"), spriteIjimExtraRow_);

    auto* const offsetX = makeFloatControl(formPanel, "spr.offset.x");
    auto* const offsetY = makeFloatControl(formPanel, "spr.offset.y");
    auto* const offsetZ = makeFloatControl(formPanel, "spr.offset.z");
    spriteOffset_ = { offsetX, offsetY, offsetZ };
    form->addRow(QStringLiteral("Offset X"), offsetX);
    form->addRow(QStringLiteral("Offset Y"), offsetY);
    form->addRow(QStringLiteral("Offset Z"), offsetZ);
    const std::array<QString, 3> offsetHelp{
        QStringLiteral("Moves the billboard along camera right. This direction turns with the view."),
        QStringLiteral("Moves the billboard along camera depth. In an orthographic view, this does not change its on-screen position or size."),
        QStringLiteral("Moves the billboard along camera up. This direction turns with the view.") };
    for ( std::size_t axis = 0; axis < spriteOffset_.size(); ++axis )
    {
        spriteOffset_[axis]->setToolTip(offsetHelp[axis]);
        spriteOffset_[axis]->lineEdit()->setToolTip(offsetHelp[axis]);
        if ( QWidget* const label = form->labelForField(spriteOffset_[axis]) )
        {
            label->setToolTip(offsetHelp[axis]);
        }
    }

    auto* const formScroll = new QScrollArea(splitter);
    formScroll->setWidgetResizable(true);
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setWidget(formPanel);
    formScroll->setMinimumWidth(embedded_ ? 250 : 420);

    auto* const previewPanel = new QWidget(splitter);
    auto* const previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    auto* const previewHeader = new QHBoxLayout();
    auto* const previewTitle = new QLabel(QStringLiteral("3D Preview"), previewPanel);
    previewTitle->setStyleSheet(QStringLiteral("font-weight: 600"));
    previewHeader->addWidget(previewTitle);
    previewHeader->addStretch(1);
    previewLayout->addLayout(previewHeader);

    auto* const previewControls = new QVBoxLayout();
    previewControls->setContentsMargins(0, 0, 0, 0);
    previewControls->setSpacing(4);
    auto* const previewDisplayControls = new QHBoxLayout();
    previewDisplayControls->setContentsMargins(0, 0, 0, 0);
    auto* const previewPlaybackControls = new QHBoxLayout();
    previewPlaybackControls->setContentsMargins(0, 0, 0, 0);
    spritePreviewBoundsCheck_ = new QCheckBox(QStringLiteral("Bounds"), previewPanel);
    spritePreviewBoundsCheck_->setObjectName(QStringLiteral("spr.preview.bounds"));
    spritePreviewBoundsCheck_->setChecked(true);
    spritePreviewBoundsCheck_->setToolTip(QStringLiteral(
        "Bounds encloses the sprite from every camera angle. XYZ offsets can enlarge it; it is not the current image rectangle. IJIM type 2 follows the oriented image and ignores offsets."));
    spritePreviewFpsSlider_ = new QSlider(Qt::Horizontal, previewPanel);
    spritePreviewFpsSlider_->setObjectName(QStringLiteral("spr.preview.fps"));
    spritePreviewFpsSlider_->setRange(0, 99);
    spritePreviewFpsSlider_->setSingleStep(1);
    spritePreviewFpsSlider_->setPageStep(10);
    spritePreviewFpsSlider_->setValue(12);
    spritePreviewFpsSlider_->setMinimumWidth(64);
    spritePreviewFpsSlider_->setToolTip(QStringLiteral("Sprite animation speed; 0 pauses playback"));
    spritePreviewFpsLabel_ = new QLabel(QStringLiteral("FPS 12"), previewPanel);
    spritePreviewFpsLabel_->setObjectName(QStringLiteral("spr.preview.fps.label"));
    spritePreviewFpsLabel_->setMinimumWidth(48);
    spritePreviewFpsLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    spritePreviewPauseButton_ = new QToolButton(previewPanel);
    spritePreviewPauseButton_->setObjectName(QStringLiteral("spr.preview.pause"));
    spritePreviewPauseButton_->setCheckable(true);
    spritePreviewPauseButton_->setText(QStringLiteral("Pause"));
    spritePreviewPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    spritePreviewPauseButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    spritePreviewPauseButton_->setToolTip(QStringLiteral("Pause animation"));
    spritePreviewPauseButton_->setFocusPolicy(Qt::NoFocus);
    spritePreviewStopButton_ = new QToolButton(previewPanel);
    spritePreviewStopButton_->setObjectName(QStringLiteral("spr.preview.stop"));
    spritePreviewStopButton_->setText(QStringLiteral("Stop"));
    spritePreviewStopButton_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    spritePreviewStopButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    spritePreviewStopButton_->setToolTip(QStringLiteral("Return to the first cel and pause"));
    spritePreviewStopButton_->setFocusPolicy(Qt::NoFocus);
    spritePreviewRecenterButton_ = makeButton(
        QStringLiteral("Recenter"), "spr.preview.recenter");
    previewFitButton_ = spritePreviewRecenterButton_;
    previewFitButton_->setParent(previewPanel);
    previewDisplayControls->addWidget(spritePreviewBoundsCheck_);
    previewDisplayControls->addWidget(spritePreviewFpsSlider_, 1);
    previewDisplayControls->addWidget(spritePreviewFpsLabel_);
    previewPlaybackControls->addWidget(spritePreviewPauseButton_);
    previewPlaybackControls->addWidget(spritePreviewStopButton_);
    previewPlaybackControls->addStretch(1);
    previewPlaybackControls->addWidget(spritePreviewRecenterButton_);
    previewControls->addLayout(previewDisplayControls);
    previewControls->addLayout(previewPlaybackControls);
    previewLayout->addLayout(previewControls);

    auto* const previewView = new GlView(previewPanel);
    auto* const previewDoubleClickFilter = new DoubleClickPreviewFilter(previewView);
    previewDoubleClickFilter->doubleClickCallback = [this]()
    {
        pickSpriteMaterial();
    };
    previewView->installEventFilter(previewDoubleClickFilter);
    previewView_ = previewView;
    previewView_->setObjectName(QStringLiteral("spr.preview.gl_view"));
    previewView_->setResourceLocator(previewResources_.get());
    previewView_->setTemplateDatabase(&previewTemplates_);
    previewView_->setRenderPipeline(GlView::RenderPipeline::Editor);
    previewView_->resetPreviewViewportState();
    previewView_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    previewView_->setShowGrid(false);
    previewView_->setShowWireframe(false);
    previewView_->setShowVertices(false);
    previewView_->setShowSolid(true);
    previewView_->setShowTextures(true);
    previewView_->setShowFog(false);
    previewView_->setShowThings(true);
    previewView_->setShowHiddenThings(false);
    previewView_->setShowEditorCubes(false);
    previewView_->setShowPreviewCubes(false);
    previewView_->setShowThingGizmos(false);
    previewView_->setShowThingCollisionShapes(false);
    previewView_->setShowThingVisualBounds(true);
    previewView_->setShowTemplateCollisionBoundsGuide(false);
    previewView_->setShowAxisWidget(false);
    previewView_->setShowToastOverlays(false);
    previewView_->setThingViewMode(GlView::ThingViewMode::Shaded);
    previewView_->setLightingMode(GlView::LightingMode::Gouraud);
    previewView_->setViewportPickingEnabled(false);
    previewView_->setHoverPickEnabled(false);
    previewView_->setPreviewInteractionEnabled(true);
    previewView_->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    previewView_->setProperty(
        "resource_authoring.preview_cmp",
        QString::fromStdString(context_.masterCmp));
    previewView_->setProperty("spr.preview.isolated_resource_locator", true);
    previewView_->setProperty("spr.preview.isolated_resource", true);
    previewView_->setProperty("spr.preview.resource_name", QStringLiteral("draft.spr"));
    previewView_->setMinimumSize(embedded_ ? QSize(250, 220) : QSize(420, 360));
    previewLayout->addWidget(previewView_, 1);
    connect(previewView_, &GlView::resourceFilesChanged, this, [this]()
        {
            spritePreviewCacheKey_.clear();
            scheduleSpritePreview();
        });
    previewTimer_->setInterval(33);
    previewTimer_->setTimerType(Qt::PreciseTimer);

    splitter->addWidget(formScroll);
    splitter->addWidget(previewPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    contentLayout_->addWidget(splitter);

    connect(spriteMaterial_, &QLineEdit::textChanged, this, [this](const QString& text)
        {
            if ( refreshing_ )
            {
                return;
            }
            if ( const auto material = ui::episode::gameBytesForDisplayText(text) )
            {
                sprite_.material = *material;
            }
            scheduleSpritePreview();
            markDirty();
        });
    connect(materialPickButton_, &QToolButton::clicked, this, [this]()
        {
            pickSpriteMaterial();
        });
    connect(spritePreviewRecenterButton_, &QPushButton::clicked, this, [this]()
        {
            if ( previewView_ && previewAttached_ )
            {
                const bool fitted = previewView_->fitSpritePreviewToLevel();
                const bool wasUnavailable = previewView_->property("spr.preview.fit_unavailable").toBool();
                previewView_->setProperty("spr.preview.fit_unavailable", !fitted);
                if ( !fitted )
                {
                    setStatus(QStringLiteral("Sprite preview exceeds the camera's depth range."), true);
                }
                else if ( wasUnavailable )
                {
                    setStatus(dirty_ ? QStringLiteral("Unsaved changes.") : QStringLiteral("Ready."));
                }
            }
        });
    connect(spritePreviewBoundsCheck_, &QCheckBox::toggled, this, [this](const bool enabled)
        {
            if ( previewView_ )
            {
                previewView_->setShowThingVisualBounds(enabled);
            }
        });

    const auto updatePauseButton = [this]()
    {
        if ( !spritePreviewPauseButton_ )
        {
            return;
        }
        spritePreviewPauseButton_->setText(
            spriteAnimationPaused_ ? QStringLiteral("Play") : QStringLiteral("Pause"));
        spritePreviewPauseButton_->setIcon(style()->standardIcon(
            spriteAnimationPaused_ ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
        spritePreviewPauseButton_->setToolTip(
            spriteAnimationPaused_
                ? QStringLiteral("Play animation")
                : QStringLiteral("Pause animation"));
    };
    updatePauseButton();
    connect(spritePreviewPauseButton_, &QToolButton::toggled, this,
        [this, updatePauseButton](const bool paused)
        {
            spriteAnimationPaused_ = paused;
            spriteAnimationFrameRemainder_ = 0.0;
            updatePauseButton();
            updateSpriteAnimationTimer();
        });

    const auto showSpritePreviewCel = [this](const int index)
    {
        if ( !previewView_
            || !previewAttached_
            || index < 0
            || static_cast<std::size_t>(index) >= spritePreviewCels_.size() )
        {
            return false;
        }
        const SpritePreviewMatCel& cel =
            spritePreviewCels_[static_cast<std::size_t>(index)];
        if ( !previewView_->updatePreviewMaterialTexture(
                sprite_.material,
                previewLevel_.getMasterCMP(),
                cel.image,
                cel.alphaBpp,
                cel.solidColor,
                cel.classicSolidLightRamp) )
        {
            return false;
        }
        spritePreviewCelIndex_ = index;
        return true;
    };
    connect(spritePreviewStopButton_, &QToolButton::clicked, this,
        [this, updatePauseButton, showSpritePreviewCel]()
        {
            spriteAnimationPaused_ = true;
            spritePreviewCelIndex_ = 0;
            spriteAnimationFrameRemainder_ = 0.0;
            if ( previewAttached_ && !spritePreviewCels_.empty() )
            {
                showSpritePreviewCel(0);
            }
            if ( spritePreviewPauseButton_ )
            {
                const QSignalBlocker blocker(spritePreviewPauseButton_);
                spritePreviewPauseButton_->setChecked(true);
            }
            updatePauseButton();
            updateSpriteAnimationTimer();
        });

    spriteAnimationTimer_ = new QTimer(this);
    spriteAnimationTimer_->setTimerType(Qt::PreciseTimer);
    connect(spriteAnimationTimer_, &QTimer::timeout, this,
        [this, showSpritePreviewCel]()
        {
            if ( !previewView_
                || !previewAttached_
                || !isVisible()
                || !previewView_->isVisible()
                || spritePreviewCels_.size() < 2
                || spriteAnimationPaused_
                || spritePreviewFramesPerSecond() <= 0 )
            {
                updateSpriteAnimationTimer();
                return;
            }

            const qint64 elapsed = spriteAnimationClock_.elapsed();
            const qint64 deltaMs = std::max<qint64>(
                0, elapsed - spriteAnimationLastElapsedMs_);
            spriteAnimationLastElapsedMs_ = elapsed;
            const int frameCount = static_cast<int>(spritePreviewCels_.size());
            const int fps = spritePreviewFramesPerSecond();
            const SpriteAnimationClockStep clockStep = spriteAnimationClockStep(
                deltaMs, fps, spriteAnimationFrameRemainder_, frameCount);
            spriteAnimationFrameRemainder_ = clockStep.remainder;
            const int frameAdvance = clockStep.advance;
            if ( frameAdvance != 0 )
            {
                const int nextIndex = static_cast<int>(
                    (static_cast<std::int64_t>(spritePreviewCelIndex_)
                        + static_cast<std::int64_t>(frameAdvance % frameCount))
                    % frameCount);
                showSpritePreviewCel(nextIndex);
            }

            // An early tick can advance zero frames. Retry at the actual next
            // boundary instead of skipping a complete two-cel period on the
            // following late tick. Texture upload time counts toward it too.
            const qint64 millisecondsSinceAdvance = std::max<qint64>(
                0, spriteAnimationClock_.elapsed() - elapsed);
            const int nextDelayMs = spriteAnimationNextDelayMs(
                fps, spriteAnimationFrameRemainder_, millisecondsSinceAdvance);
            if ( nextDelayMs > 0 )
            {
                spriteAnimationTimer_->start(nextDelayMs);
            }
            else
            {
                spriteAnimationTimer_->stop();
            }
        });
    connect(spritePreviewFpsSlider_, &QSlider::valueChanged, this,
        [this](const int fps)
        {
            if ( spritePreviewFpsLabel_ )
            {
                spritePreviewFpsLabel_->setText(
                    QStringLiteral("FPS %1").arg(fps));
            }
            spriteAnimationFrameRemainder_ = 0.0;
            updateSpriteAnimationTimer();
        });

    connect(spriteTypeCombo_, &QComboBox::currentIndexChanged, this, [this](const int index)
        {
            if ( refreshing_ || index < 0 )
            {
                return;
            }
            sprite_.type = spriteTypeCombo_->currentData().toInt();
            updateSpriteProfileRows();
            scheduleSpritePreview();
            markDirty();
        });
    struct FloatEditGestureState
    {
        double baseline{};
        bool active{};
    };
    const auto bindFloatControl = [this](
        ui::ScrubFloatEdit* control,
        const QString& pendingKey,
        const std::function<double()>& readModel,
        const std::function<void(double)>& writeModel)
    {
        control->lineEdit()->setMaxLength(4096);
        control->lineEdit()->setProperty(
            "resource_authoring.sprite_numeric_key", pendingKey);
        control->lineEdit()->installEventFilter(this);
        connect(control->lineEdit(), &QLineEdit::textEdited, this,
            [this, pendingKey](const QString& text)
            {
                if ( !refreshing_ && setSpritePendingNumericEdit(pendingKey, text) )
                {
                    markDirty();
                }
            });
        connect(control->lineEdit(), &QLineEdit::editingFinished, this,
            [this, pendingKey]()
            {
                if ( clearSpritePendingNumericEdit(pendingKey) )
                {
                    markDirty();
                }
            });

        const auto gesture = std::make_shared<FloatEditGestureState>();
        const auto applyValue = [this, gesture, pendingKey, readModel, writeModel](
            const double value,
            const bool commitOutsideGesture)
        {
            if ( refreshing_ )
            {
                return;
            }
            const double normalizedValue =
                static_cast<double>(static_cast<float>(value));
            if ( readModel() == normalizedValue )
            {
                if ( commitOutsideGesture )
                {
                    if ( clearSpritePendingNumericEdit(pendingKey) )
                    {
                        markDirty();
                    }
                }
                return;
            }
            writeModel(normalizedValue);
            scheduleSpritePreview();
            const bool committed = !gesture->active && commitOutsideGesture;
            const bool pendingChanged = commitOutsideGesture
                && clearSpritePendingNumericEdit(pendingKey);
            if ( committed || pendingChanged )
            {
                markDirty();
            }
        };
        connect(control, &ui::ScrubFloatEdit::scrubStarted, this,
            [gesture, readModel](const double)
            {
                gesture->baseline = readModel();
                gesture->active = true;
            });
        connect(control, &ui::ScrubFloatEdit::scrubValueChanged, this,
            [applyValue](const double value)
            {
                applyValue(value, false);
            });
        connect(control, &ui::ScrubFloatEdit::valueCommitted, this,
            [applyValue](const double value)
            {
                applyValue(value, true);
            });
        connect(control, &ui::ScrubFloatEdit::scrubFinished, this,
            [this, gesture, readModel, pendingKey](const double)
            {
                gesture->active = false;
                const bool changed = readModel() != gesture->baseline;
                if ( changed )
                {
                    scheduleSpritePreview();
                }
                const bool pendingChanged = clearSpritePendingNumericEdit(pendingKey);
                if ( changed || pendingChanged )
                {
                    markDirty();
                }
            });
        connect(control, &ui::ScrubFloatEdit::scrubCanceled, this,
            [this, gesture, control, readModel, writeModel, pendingKey]()
            {
                const double currentValue = readModel();
                if ( currentValue != gesture->baseline )
                {
                    writeModel(gesture->baseline);
                    scheduleSpritePreview();
                }
                gesture->active = false;
                {
                    const QSignalBlocker blocker(control);
                    control->setValue(gesture->baseline);
                }
                if ( clearSpritePendingNumericEdit(pendingKey) )
                {
                    markDirty();
                }
            });
    };
    bindFloatControl(
        spriteWidth_,
        QStringLiteral("width"),
        [this]() { return static_cast<double>(sprite_.width); },
        [this](const double value) { sprite_.width = static_cast<float>(value); });
    bindFloatControl(
        spriteHeight_,
        QStringLiteral("height"),
        [this]() { return static_cast<double>(sprite_.height); },
        [this](const double value) { sprite_.height = static_cast<float>(value); });
    connect(spriteGeometryMode_, &QComboBox::currentIndexChanged, this, [this](const int index)
        {
            if ( !refreshing_ && index >= 0
                && spriteGeometryMode_->currentData().isValid() )
            {
                sprite_.geometryMode = spriteGeometryMode_->currentData().toInt();
                scheduleSpritePreview();
                markDirty();
            }
        });
    connect(spriteLightMode_, &QComboBox::currentIndexChanged, this, [this](const int index)
        {
            if ( !refreshing_ && index >= 0
                && spriteLightMode_->currentData().isValid() )
            {
                sprite_.lightMode = spriteLightMode_->currentData().toInt();
                scheduleSpritePreview();
                markDirty();
            }
        });
    connect(spriteTextureMode_, &QComboBox::currentIndexChanged, this, [this](const int index)
        {
            if ( !refreshing_ && index >= 0
                && spriteTextureMode_->currentData().isValid() )
            {
                sprite_.textureMode = spriteTextureMode_->currentData().toInt();
                scheduleSpritePreview();
                markDirty();
            }
        });
    struct ExtraScalarGestureState
    {
        std::array<float, 4> baseline{};
        bool active{};
    };
    const auto extraScalarGesture = std::make_shared<ExtraScalarGestureState>();
    const QString extraScalarPendingKey = QStringLiteral("extraLightScalar");
    spriteExtraScalar_->lineEdit()->setMaxLength(4096);
    spriteExtraScalar_->lineEdit()->setProperty(
        "resource_authoring.sprite_numeric_key", extraScalarPendingKey);
    spriteExtraScalar_->lineEdit()->installEventFilter(this);
    connect(spriteExtraScalar_->lineEdit(), &QLineEdit::textEdited, this,
        [this, extraScalarPendingKey](const QString& text)
        {
            if ( !refreshing_ && setSpritePendingNumericEdit(extraScalarPendingKey, text) )
            {
                markDirty();
            }
        });
    connect(spriteExtraScalar_->lineEdit(), &QLineEdit::editingFinished, this,
        [this, extraScalarPendingKey]()
        {
            if ( clearSpritePendingNumericEdit(extraScalarPendingKey) )
            {
                markDirty();
            }
        });
    const auto applyExtraScalar = [this, extraScalarGesture, extraScalarPendingKey](
        const double value,
        const bool commitOutsideGesture)
    {
        if ( refreshing_ )
        {
            return;
        }
        const float channel = static_cast<float>(value);
        const std::array<float, 4> next{
            channel,
            channel,
            channel,
            sprite_.extraLightRGBA[3] };
        if ( sprite_.extraLightRGBA == next )
        {
            if ( commitOutsideGesture )
            {
                if ( clearSpritePendingNumericEdit(extraScalarPendingKey) )
                {
                    markDirty();
                }
            }
            return;
        }
        sprite_.extraLightRGBA = next;
        scheduleSpritePreview();
        const bool committed = !extraScalarGesture->active && commitOutsideGesture;
        const bool pendingChanged = commitOutsideGesture
            && clearSpritePendingNumericEdit(extraScalarPendingKey);
        if ( committed || pendingChanged )
        {
            markDirty();
        }
    };
    connect(spriteExtraScalar_, &ui::ScrubFloatEdit::scrubStarted, this,
        [this, extraScalarGesture](const double)
        {
            extraScalarGesture->baseline = sprite_.extraLightRGBA;
            extraScalarGesture->active = true;
        });
    connect(spriteExtraScalar_, &ui::ScrubFloatEdit::scrubValueChanged, this,
        [applyExtraScalar](const double value)
        {
            applyExtraScalar(value, false);
        });
    connect(spriteExtraScalar_, &ui::ScrubFloatEdit::valueCommitted, this,
        [applyExtraScalar](const double value)
        {
            applyExtraScalar(value, true);
        });
    connect(spriteExtraScalar_, &ui::ScrubFloatEdit::scrubFinished, this,
        [this, extraScalarGesture, extraScalarPendingKey](const double)
        {
            extraScalarGesture->active = false;
            const bool changed = sprite_.extraLightRGBA != extraScalarGesture->baseline;
            if ( changed )
            {
                scheduleSpritePreview();
            }
            const bool pendingChanged = clearSpritePendingNumericEdit(extraScalarPendingKey);
            if ( changed || pendingChanged )
            {
                markDirty();
            }
        });
    connect(spriteExtraScalar_, &ui::ScrubFloatEdit::scrubCanceled, this,
        [this, extraScalarGesture, extraScalarPendingKey]()
        {
            if ( sprite_.extraLightRGBA != extraScalarGesture->baseline )
            {
                sprite_.extraLightRGBA = extraScalarGesture->baseline;
                scheduleSpritePreview();
            }
            extraScalarGesture->active = false;
            {
                const QSignalBlocker blocker(spriteExtraScalar_);
                spriteExtraScalar_->setValue(sprite_.extraLightRGBA[0]);
            }
            if ( clearSpritePendingNumericEdit(extraScalarPendingKey) )
            {
                markDirty();
            }
        });
    connect(extraLightSwatch_, &QPushButton::clicked, this, [this]()
        {
            openExtraLightPopup();
        });
    for ( std::size_t index = 0; index < spriteOffset_.size(); ++index )
    {
        static constexpr std::array<const char*, 3> offsetKeys{
            "offsetX", "offsetY", "offsetZ" };
        bindFloatControl(
            spriteOffset_[index],
            QString::fromLatin1(offsetKeys[index]),
            [this, index]() { return static_cast<double>(sprite_.offsetXYZ[index]); },
            [this, index](const double value)
            {
                sprite_.offsetXYZ[index] = static_cast<float>(value);
            });
    }
}

ui::ScrubFloatEdit* ResourceAuthoringDialog::makeFloatControl(
    QWidget* parent,
    const char* objectName)
{
    auto* const edit = new ui::ScrubFloatEdit(parent);
    edit->setObjectName(QString::fromLatin1(objectName));
    const double floatLimit = static_cast<double>(std::numeric_limits<float>::max());
    edit->setRange(-floatLimit, floatLimit);
    edit->setDecimals(9);
    edit->setSingleStep(0.1);
    edit->setStepperButtonWidth(12);
    edit->setStepperUsesScrubSignals(true);
    return edit;
}

QColor ResourceAuthoringDialog::displayExtraLightColor() const
{
    const auto displayChannel = [](const float channel)
    {
        return std::clamp(static_cast<double>(channel), 0.0, 1.0);
    };
    return QColor::fromRgbF(
        displayChannel(sprite_.extraLightRGBA[0]),
        displayChannel(sprite_.extraLightRGBA[1]),
        displayChannel(sprite_.extraLightRGBA[2]),
        displayChannel(sprite_.extraLightRGBA[3]));
}

void ResourceAuthoringDialog::openExtraLightPopup()
{
    if ( extraLightPopup_ )
    {
        extraLightPopup_->raise();
        return;
    }
    const QColor initialColor = displayExtraLightColor();
    auto* const popup = new ui::LiveColorPopup(
        this,
        initialColor,
        true,
        [this, initialColor](const QColor& color)
        {
            if ( color == initialColor )
            {
                previewExtraLight_.reset();
            }
            else
            {
                previewExtraLight_ = std::array<float, 4>{
                    static_cast<float>(color.redF()),
                    static_cast<float>(color.greenF()),
                    static_cast<float>(color.blueF()),
                    static_cast<float>(color.alphaF()) };
            }
            if ( extraLightSwatch_ )
            {
                ui::configureColorSwatchButton(extraLightSwatch_, color);
            }
            scheduleSpritePreview();
        },
        [this]()
        {
            const std::optional<std::array<float, 4>> staged = previewExtraLight_;
            previewExtraLight_.reset();
            const bool changed = !extraLightPopupEscaped_
                && staged && *staged != sprite_.extraLightRGBA;
            if ( changed )
            {
                sprite_.extraLightRGBA = *staged;
            }
            extraLightPopupEscaped_ = false;
            extraLightPopup_.clear();
            if ( extraLightSwatch_ )
            {
                ui::configureColorSwatchButton(
                    extraLightSwatch_, displayExtraLightColor());
            }
            scheduleSpritePreview();
            if ( changed )
            {
                markDirty();
            }
        });
    popup->setObjectName(QStringLiteral("spr.extra_light.popup"));
    popup->installEventFilter(this);
    for ( QWidget* const child : popup->findChildren<QWidget*>() )
    {
        child->installEventFilter(this);
    }
    extraLightPopup_ = popup;
    extraLightPopupEscaped_ = false;
    popup->showFor(extraLightSwatch_);
}

void ResourceAuthoringDialog::updateSpriteProfileRows()
{
    const bool ijim = spriteProfile_ == assets::SprProfile::ijim;
    spriteGeometryMode_->setToolTip(QStringLiteral(
        "Selects no drawing, corner vertices, the four outer wireframe edges, the MAT Solid color or the MAT texture. This authored value is retained by both project profiles; the isolated preview follows it."));
    spriteLightMode_->setToolTip(ijim
        ? QStringLiteral("None (0) renders full brightness and ignores extra-light RGB. Lit and Diffuse use extra-light RGB; the IJIM sprite renderer also draws Gouraud as Diffuse. Alpha still controls opacity.")
        : QStringLiteral("Fully lit (0) ignores extra-light brightness. Other sprite light modes use scalar extra light. This isolated preview has no ambient illumination."));
    spriteTextureMode_->setToolTip(ijim
        ? QStringLiteral("Retained in the SPR file; the IJIM loader ignores texture mode.")
        : QStringLiteral("Legacy texture mapping mode. A camera-facing billboard has one camera depth, so affine and perspective mapping produce the same visible card."));
    const QString extraScalarHelp = QStringLiteral("Extra light intensity");
    spriteExtraScalar_->setToolTip(extraScalarHelp);
    spriteExtraScalar_->lineEdit()->setToolTip(extraScalarHelp);
    extraLightSwatch_->setToolTip(QStringLiteral(
        "Extra light color (RGBA). RGB lights the sprite except in None (0). Alpha controls opacity in every light mode."));
    QFormLayout* const form = spriteClassicExtraRow_
        && spriteClassicExtraRow_->parentWidget()
        ? qobject_cast<QFormLayout*>(spriteClassicExtraRow_->parentWidget()->layout())
        : nullptr;
    if ( form )
    {
        form->setRowVisible(spriteGeometryMode_, true);
        form->setRowVisible(spriteTextureMode_, !ijim);
        form->setRowVisible(spriteClassicExtraRow_, !ijim);
        form->setRowVisible(spriteIjimExtraRow_, ijim);
    }
    else
    {
        spriteGeometryMode_->setVisible(true);
        spriteTextureMode_->setVisible(!ijim);
        spriteClassicExtraRow_->setVisible(!ijim);
        spriteIjimExtraRow_->setVisible(ijim);
    }
    for ( ui::ScrubFloatEdit* const offset : spriteOffset_ )
    {
        const bool visible = !ijim || sprite_.type != 2;
        if ( form )
        {
            form->setRowVisible(offset, visible);
        }
        else
        {
            offset->setVisible(visible);
        }
    }

    const QSignalBlocker typeBlocker(spriteTypeCombo_);
    const QSignalBlocker lightBlocker(spriteLightMode_);
    const QSignalBlocker textureBlocker(spriteTextureMode_);
    spriteTextureMode_->clear();
    spriteTextureMode_->addItem(QStringLiteral("Affine (0)"), 0);
    spriteTextureMode_->addItem(QStringLiteral("Perspective (1)"), 1);
    spriteTextureMode_->addItem(QStringLiteral("Legacy / unknown (2)"), 2);
    spriteTextureMode_->addItem(QStringLiteral("Legacy / unknown (3)"), 3);
    int textureIndex = spriteTextureMode_->findData(sprite_.textureMode);
    if ( textureIndex < 0 )
    {
        spriteTextureMode_->addItem(
            QStringLiteral("Unknown current value (%1)").arg(sprite_.textureMode), sprite_.textureMode);
        textureIndex = spriteTextureMode_->count() - 1;
    }
    spriteTextureMode_->setCurrentIndex(textureIndex);
    spriteTypeCombo_->clear();
    if ( ijim )
    {
        spriteTypeCombo_->addItem(QStringLiteral("Type 0 (camera-facing)"), 0);
        spriteTypeCombo_->addItem(QStringLiteral("Type 2 (oriented)"), 2);
    }
    else
    {
        spriteTypeCombo_->addItem(QStringLiteral("Type 0 (billboard)"), 0);
        spriteTypeCombo_->addItem(QStringLiteral("Type 1 (billboard)"), 1);
        spriteTypeCombo_->addItem(QStringLiteral("Type 2 (billboard)"), 2);
    }
    int typeIndex = spriteTypeCombo_->findData(sprite_.type);
    if ( typeIndex < 0 )
    {
        spriteTypeCombo_->addItem(
            QStringLiteral("Unsupported current value (%1)").arg(sprite_.type),
            sprite_.type);
        typeIndex = spriteTypeCombo_->count() - 1;
    }
    spriteTypeCombo_->setCurrentIndex(typeIndex);

    spriteLightMode_->clear();
    if ( ijim )
    {
        spriteLightMode_->addItem(QStringLiteral("None"), 0);
        spriteLightMode_->addItem(QStringLiteral("Lit"), 1);
        spriteLightMode_->addItem(QStringLiteral("Diffuse"), 2);
        spriteLightMode_->addItem(QStringLiteral("Gouraud"), 3);
    }
    else
    {
        spriteLightMode_->addItem(QStringLiteral("Fully lit"), 0);
        spriteLightMode_->addItem(QStringLiteral("Not lit"), 1);
        spriteLightMode_->addItem(QStringLiteral("Diffuse"), 2);
        spriteLightMode_->addItem(QStringLiteral("Gouraud"), 3);
    }
    int lightIndex = spriteLightMode_->findData(sprite_.lightMode);
    if ( lightIndex < 0 )
    {
        spriteLightMode_->addItem(
            QStringLiteral("Unsupported current value (%1)").arg(sprite_.lightMode),
            sprite_.lightMode);
        lightIndex = spriteLightMode_->count() - 1;
    }
    spriteLightMode_->setCurrentIndex(lightIndex);
}

void ResourceAuthoringDialog::refreshSpriteWidgets()
{
    const bool previousRefreshing = refreshing_;
    refreshing_ = true;
    const QSignalBlocker materialBlocker(spriteMaterial_);
    const QSignalBlocker typeBlocker(spriteTypeCombo_);
    const QSignalBlocker widthBlocker(spriteWidth_);
    const QSignalBlocker heightBlocker(spriteHeight_);
    const QSignalBlocker geometryBlocker(spriteGeometryMode_);
    const QSignalBlocker lightBlocker(spriteLightMode_);
    const QSignalBlocker textureBlocker(spriteTextureMode_);
    const QSignalBlocker extraScalarBlocker(spriteExtraScalar_);
    spriteMaterial_->setText(ui::episode::displayTextForGameBytes(sprite_.material));
    updateSpriteProfileRows();
    spriteWidth_->setValue(sprite_.width);
    spriteHeight_->setValue(sprite_.height);
    selectIntegerCombo(spriteGeometryMode_, sprite_.geometryMode);
    selectIntegerCombo(spriteTextureMode_, sprite_.textureMode);
    spriteExtraScalar_->setValue(sprite_.extraLightRGBA[0]);
    if ( extraLightSwatch_ )
    {
        ui::configureColorSwatchButton(extraLightSwatch_, displayExtraLightColor());
    }
    for ( std::size_t index = 0; index < spriteOffset_.size(); ++index )
    {
        const QSignalBlocker blocker(spriteOffset_[index]);
        spriteOffset_[index]->setValue(sprite_.offsetXYZ[index]);
    }
    refreshing_ = previousRefreshing;
}

void ResourceAuthoringDialog::updateSpriteAnimationTimer()
{
    const bool animatedMaterial = previewAttached_ && spritePreviewCels_.size() >= 2;
    if ( spritePreviewFpsSlider_ )
    {
        spritePreviewFpsSlider_->setVisible(animatedMaterial);
    }
    if ( spritePreviewFpsLabel_ )
    {
        spritePreviewFpsLabel_->setVisible(animatedMaterial);
    }
    if ( spritePreviewPauseButton_ )
    {
        spritePreviewPauseButton_->setVisible(animatedMaterial);
    }
    if ( spritePreviewStopButton_ )
    {
        spritePreviewStopButton_->setVisible(animatedMaterial);
    }
    if ( !spriteAnimationTimer_ )
    {
        return;
    }
    if ( !isVisible()
        || !previewView_
        || !previewView_->isVisible()
        || !previewAttached_
        || spritePreviewCels_.size() < 2
        || spriteAnimationPaused_
        || spritePreviewFramesPerSecond() <= 0 )
    {
        spriteAnimationTimer_->stop();
        spriteAnimationFrameRemainder_ = 0.0;
        return;
    }

    const int fps = spritePreviewFramesPerSecond();
    const int intervalMs = spriteAnimationNextDelayMs(fps, spriteAnimationFrameRemainder_, 0);
    if ( intervalMs <= 0 )
    {
        spriteAnimationTimer_->stop();
        return;
    }
    spriteAnimationTimer_->setInterval(intervalMs);
    spriteAnimationClock_.start();
    spriteAnimationLastElapsedMs_ = 0;
    spriteAnimationTimer_->start();
}

int ResourceAuthoringDialog::spritePreviewFramesPerSecond() const
{
    return spritePreviewFpsSlider_ ? spritePreviewFpsSlider_->value() : 12;
}

void ResourceAuthoringDialog::setSpritePreviewFramesPerSecond(const int fps)
{
    if ( spritePreviewFpsSlider_ )
    {
        const int bounded = std::clamp(fps, 0, 99);
        spritePreviewFpsSlider_->setValue(bounded);
        if ( spritePreviewFpsLabel_ )
        {
            spritePreviewFpsLabel_->setText(QStringLiteral("FPS %1").arg(bounded));
        }
    }
    updateSpriteAnimationTimer();
}

QCborMap ResourceAuthoringDialog::captureSpritePresentation() const
{
    QCborMap presentation{};
    presentation.insert(QStringLiteral("version"), 1);
    presentation.insert(
        QStringLiteral("bounds"),
        spritePreviewBoundsCheck_ && spritePreviewBoundsCheck_->isChecked());
    presentation.insert(QStringLiteral("fps"), spritePreviewFramesPerSecond());
    presentation.insert(QStringLiteral("paused"), spriteAnimationPaused_);

    QCborArray splitterSizes{};
    const QSplitter* const splitter = findChild<QSplitter*>(QStringLiteral("spr.editor_splitter"));
    const QList<int> sizes = splitter ? splitter->sizes() : QList<int>{ 0, 0 };
    splitterSizes.append(std::max(0, sizes.value(0)));
    splitterSizes.append(std::max(0, sizes.value(1)));
    presentation.insert(QStringLiteral("splitterSizes"), splitterSizes);

    QCborMap cameraMap{};
    QCborArray orbit{};
    if ( previewView_ )
    {
        const GlView::RecoveryState state = previewView_->recoveryState();
        const GlView::ViewportState& camera = state.preview;
        const app::ViewportCamera::OrbitState& pose = camera.orbit;
        orbit.append(static_cast<double>(pose.center.x()));
        orbit.append(static_cast<double>(pose.center.y()));
        orbit.append(static_cast<double>(pose.center.z()));
        orbit.append(static_cast<double>(pose.pan.x()));
        orbit.append(static_cast<double>(pose.pan.y()));
        orbit.append(static_cast<double>(pose.pan.z()));
        orbit.append(static_cast<double>(pose.radius));
        orbit.append(static_cast<double>(pose.distance));
        orbit.append(static_cast<double>(pose.yawDeg));
        orbit.append(static_cast<double>(pose.pitchDeg));
        orbit.append(static_cast<double>(pose.rollDeg));
        cameraMap.insert(QStringLiteral("projectionType"), static_cast<int>(camera.projectionType));
        cameraMap.insert(QStringLiteral("fovXDeg"), static_cast<double>(camera.fovXDeg));
        cameraMap.insert(QStringLiteral("orthoScale"), static_cast<double>(camera.orthoScale));
        cameraMap.insert(QStringLiteral("clipEnabled"), camera.clipEnabled);
    }
    else
    {
        const app::ViewportCamera::OrbitState pose{};
        orbit.append(static_cast<double>(pose.center.x()));
        orbit.append(static_cast<double>(pose.center.y()));
        orbit.append(static_cast<double>(pose.center.z()));
        orbit.append(static_cast<double>(pose.pan.x()));
        orbit.append(static_cast<double>(pose.pan.y()));
        orbit.append(static_cast<double>(pose.pan.z()));
        orbit.append(static_cast<double>(pose.radius));
        orbit.append(static_cast<double>(pose.distance));
        orbit.append(static_cast<double>(pose.yawDeg));
        orbit.append(static_cast<double>(pose.pitchDeg));
        orbit.append(static_cast<double>(pose.rollDeg));
        cameraMap.insert(
            QStringLiteral("projectionType"),
            static_cast<int>(app::ViewportCamera::ProjectionType::Orthographic));
        cameraMap.insert(QStringLiteral("fovXDeg"), 90.0);
        cameraMap.insert(QStringLiteral("orthoScale"), 64.0);
        cameraMap.insert(QStringLiteral("clipEnabled"), true);
    }
    cameraMap.insert(QStringLiteral("orbit"), orbit);
    if ( !pendingSpritePresentation_.isEmpty() )
    {
        // A restored pane can be captured before its hidden preview attaches
        // and receives the saved camera after the initial refit.
        cameraMap = pendingSpritePresentation_.value(QStringLiteral("camera")).toMap();
    }
    presentation.insert(QStringLiteral("camera"), cameraMap);
    return presentation;
}

bool ResourceAuthoringDialog::validateSpritePresentation(
    const QCborMap& presentation,
    QString* const error)
{
    const auto reject = [error](const QString& message)
    {
        if ( error )
        {
            *error = message;
        }
        return false;
    };
    const auto numberInRange = [](
        const QCborValue& value,
        const double minimum,
        const double maximum,
        double& result)
    {
        if ( !value.isDouble() && !value.isInteger() )
        {
            return false;
        }
        result = value.toDouble();
        return std::isfinite(result) && result >= minimum && result <= maximum;
    };

    const QCborValue version = presentation.value(QStringLiteral("version"));
    const QCborValue bounds = presentation.value(QStringLiteral("bounds"));
    const QCborValue fps = presentation.value(QStringLiteral("fps"));
    const QCborValue paused = presentation.value(QStringLiteral("paused"));
    if ( !version.isInteger() || version.toInteger() != 1
        || !bounds.isBool()
        || !fps.isInteger() || fps.toInteger() < 0 || fps.toInteger() > 99
        || ( presentation.contains(QStringLiteral("paused")) && !paused.isBool() ) )
    {
        return reject(QStringLiteral("Sprite presentation settings are malformed."));
    }

    const QCborArray splitterSizes = presentation.value(QStringLiteral("splitterSizes")).toArray();
    if ( splitterSizes.size() != 2 )
    {
        return reject(QStringLiteral("Sprite presentation splitter sizes are malformed."));
    }
    for ( const QCborValue size : splitterSizes )
    {
        if ( !size.isInteger() || size.toInteger() < 0
            || size.toInteger() > std::numeric_limits<int>::max() )
        {
            return reject(QStringLiteral("Sprite presentation splitter sizes are out of range."));
        }
    }

    const QCborMap camera = presentation.value(QStringLiteral("camera")).toMap();
    const QCborArray orbit = camera.value(QStringLiteral("orbit")).toArray();
    if ( orbit.size() != 11 )
    {
        return reject(QStringLiteral("Sprite presentation camera orbit is malformed."));
    }
    std::array<double, 11> orbitValues{};
    for ( qsizetype index = 0; index < orbit.size(); ++index )
    {
        const double minimum = index < 6
            ? -1.0e12
            : (index < 8 ? 0.0 : -1.0e9);
        const double maximum = index < 6
            ? 1.0e12
            : (index < 8 ? 1.0e12 : 1.0e9);
        if ( !numberInRange(
                orbit.at(index),
                minimum,
                maximum,
                orbitValues[static_cast<std::size_t>(index)]) )
        {
            return reject(QStringLiteral("Sprite presentation camera orbit is out of range."));
        }
    }
    const float radius = static_cast<float>(orbitValues[6]);
    const float distance = static_cast<float>(orbitValues[7]);
    if ( !std::isfinite(radius) || radius <= 0.0f
        || !std::isfinite(distance) || distance <= 0.0f
        || orbitValues[7] < 1.0e-6 )
    {
        return reject(QStringLiteral("Sprite presentation camera distance is out of range."));
    }

    const QCborValue projectionType = camera.value(QStringLiteral("projectionType"));
    const QCborValue fovXDeg = camera.value(QStringLiteral("fovXDeg"));
    const QCborValue orthoScale = camera.value(QStringLiteral("orthoScale"));
    double fov = 0.0;
    double ortho = 0.0;
    if ( !projectionType.isInteger()
        || projectionType.toInteger() < static_cast<int>(app::ViewportCamera::ProjectionType::Perspective)
        || projectionType.toInteger() > static_cast<int>(app::ViewportCamera::ProjectionType::Orthographic)
        || !numberInRange(fovXDeg, 1.0, 179.0, fov)
        || !numberInRange(orthoScale, 0.0, 1.0e9, ortho)
        || !std::isfinite(static_cast<float>(ortho))
        || static_cast<float>(ortho) <= 0.0f
        || !camera.value(QStringLiteral("clipEnabled")).isBool() )
    {
        return reject(QStringLiteral("Sprite presentation camera settings are malformed."));
    }

    if ( error )
    {
        error->clear();
    }
    return true;
}

void ResourceAuthoringDialog::restoreSpritePresentation(const QCborMap& presentation)
{
    if ( !validateSpritePresentation(presentation, nullptr) )
    {
        return;
    }


    const QCborValue pausedValue = presentation.value(QStringLiteral("paused"));
    spriteAnimationPaused_ = pausedValue.isBool() && pausedValue.toBool();
    if ( spritePreviewPauseButton_ )
    {
        const QSignalBlocker pauseBlocker(spritePreviewPauseButton_);
        spritePreviewPauseButton_->setChecked(spriteAnimationPaused_);
        spritePreviewPauseButton_->setText(
            spriteAnimationPaused_ ? QStringLiteral("Play") : QStringLiteral("Pause"));
        spritePreviewPauseButton_->setIcon(style()->standardIcon(
            spriteAnimationPaused_ ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
        spritePreviewPauseButton_->setToolTip(
            spriteAnimationPaused_
                ? QStringLiteral("Play animation")
                : QStringLiteral("Pause animation"));
    }

    const QSignalBlocker boundsBlocker(spritePreviewBoundsCheck_);
    if ( spritePreviewBoundsCheck_ )
    {
        spritePreviewBoundsCheck_->setChecked(
            presentation.value(QStringLiteral("bounds")).toBool());
    }
    if ( previewView_ )
    {
        previewView_->setShowThingVisualBounds(
            spritePreviewBoundsCheck_ && spritePreviewBoundsCheck_->isChecked());
    }
    setSpritePreviewFramesPerSecond(
        static_cast<int>(presentation.value(QStringLiteral("fps")).toInteger(12)));

    if ( QSplitter* const splitter = findChild<QSplitter*>(QStringLiteral("spr.editor_splitter")) )
    {
        const QCborArray sizes = presentation.value(QStringLiteral("splitterSizes")).toArray();
        if ( sizes.size() == 2 )
        {
            splitter->setSizes({
                static_cast<int>(sizes.at(0).toInteger()),
                static_cast<int>(sizes.at(1).toInteger()),
            });
        }
    }
    pendingSpritePresentation_ = presentation;
}

void ResourceAuthoringDialog::applyPendingSpritePresentationAfterRefit()
{
    if ( pendingSpritePresentation_.isEmpty() || !previewView_ || !previewAttached_ )
    {
        return;
    }

    const QCborMap cameraMap = pendingSpritePresentation_.value(QStringLiteral("camera")).toMap();
    const QCborArray orbit = cameraMap.value(QStringLiteral("orbit")).toArray();
    if ( orbit.size() != 11 )
    {
        return;
    }

    const auto asFloat = [&orbit](const qsizetype index)
    {
        return static_cast<float>(orbit.at(index).toDouble());
    };
    GlView::RecoveryState state = previewView_->recoveryState();
    app::ViewportCamera::OrbitState& pose = state.preview.orbit;
    pose.center = QVector3D(asFloat(0), asFloat(1), asFloat(2));
    pose.pan = QVector3D(asFloat(3), asFloat(4), asFloat(5));
    pose.radius = asFloat(6);
    pose.distance = asFloat(7);
    pose.yawDeg = asFloat(8);
    pose.pitchDeg = asFloat(9);
    pose.rollDeg = asFloat(10);
    state.preview.projectionType = static_cast<app::ViewportCamera::ProjectionType>(
        cameraMap.value(QStringLiteral("projectionType")).toInteger());
    state.preview.fovXDeg = static_cast<float>(
        cameraMap.value(QStringLiteral("fovXDeg")).toDouble());
    state.preview.orthoScale = static_cast<float>(
        cameraMap.value(QStringLiteral("orthoScale")).toDouble());
    state.preview.clipEnabled = cameraMap.value(QStringLiteral("clipEnabled")).toBool();
    bool fitted = false;
    {
        // Native presentation owns zoom and pose. Derive only the depth
        // interval against that camera before publishing its restored state.
        const QSignalBlocker viewBlocker(previewView_);
        previewView_->restoreRecoveryState(state, false);
        fitted = previewView_->fitSpritePreviewToLevel(true);
        if ( fitted )
        {
            const GlView::RecoveryState depthFit = previewView_->recoveryState();
            state.nearPlane = depthFit.nearPlane;
            state.farPlane = depthFit.farPlane;
            state.preview.orbit.distance = depthFit.preview.orbit.distance;
        }
    }
    previewView_->restoreRecoveryState(state, false);
    previewView_->setProperty("spr.preview.fit_unavailable", !fitted);
    pendingSpritePresentation_ = QCborMap{};
}

void ResourceAuthoringDialog::pickSpriteMaterial()
{
    QPointer<ResourceAuthoringDialog> self(this);
    const std::shared_ptr<const assets::ResourceLocator> locatorSnapshot = context_.resources;
    const std::uint64_t authoredRevision = authoredRevision_;
    const QCborMap authoredState = authoredContentState();
    ResourcePickerOptions options{};
    options.kind = ResourcePickerKind::material;
    options.initialName = sprite_.material;
    if ( !context_.projectDirectory.empty() )
    {
        options.preferredDiskRoot = context_.projectDirectory;
        options.projectDirectory = context_.projectDirectory;
    }
    options.title = "Material Picker";
    options.previewCmp = context_.masterCmp;
    options.previewProjectType = context_.projectType;
    const std::optional<std::string> selected = pickResource(nullptr, locatorSnapshot, options);
    if ( !self
        || !selected
        || !self->authoringSnapshotStillCurrent(authoredRevision, authoredState) )
    {
        return;
    }
    if ( *selected == self->sprite_.material )
    {
        return;
    }
    self->spriteMaterial_->setText(ui::episode::displayTextForGameBytes(*selected));
}

void ResourceAuthoringDialog::scheduleSpritePreview(const bool refit)
{
    if ( !previewView_ || kind_ != ResourceAuthoringKind::sprite )
    {
        return;
    }
    previewRefitPending_ = previewRefitPending_ || refit;
    if ( !previewTimer_->isActive() )
    {
        previewTimer_->start();
    }
}

void ResourceAuthoringDialog::rebuildSpritePreview()
{
    if ( !previewView_ || !previewResources_ )
    {
        if ( spriteAnimationTimer_ )
        {
            spriteAnimationTimer_->stop();
        }
        return;
    }

    // Draft SPR overrides advance the preview locator's archive identity.
    // MAT/CMP dependencies belong to the unchanged source resource context.
    const std::string newCacheKey = spritePreviewMaterialCacheKey(
        context_.resources.get(),
        context_.projectType,
        context_.masterCmp,
        sprite_.material);
    const bool materialChanged = newCacheKey != spritePreviewCacheKey_;
    if ( materialChanged )
    {
        const std::vector<DecodedSpritePreviewCel> decodedCels =
            loadSpritePreviewCels(
                previewResources_.get(),
                context_.projectType,
                context_.masterCmp,
                sprite_.material);
        spritePreviewCels_.clear();
        spritePreviewCels_.reserve(decodedCels.size());
        for ( const DecodedSpritePreviewCel& cel : decodedCels )
        {
            spritePreviewCels_.push_back(SpritePreviewMatCel{
                .image = cel.image,
                .alphaBpp = cel.alphaBpp,
                .solidColor = cel.solidColor,
                .classicSolidLightRamp = cel.classicSolidLightRamp,
            });
        }
        spritePreviewCacheKey_ = newCacheKey;
        spritePreviewCelIndex_ = 0;
        spriteAnimationFrameRemainder_ = 0.0;
    }

    assets::SprFile previewSprite = sprite_;
    if (previewExtraLight_)
    {
        previewSprite.extraLightRGBA = *previewExtraLight_;
    }
    std::string serialized{};
    try
    {
        serialized = assets::saveToSPRText(previewSprite, spriteProfile_);
    }
    catch (const std::exception& exception)
    {
        if ( previewAttached_ )
        {
            previewView_->setLevel(nullptr, GlView::SetLevelMode::keepView, nullptr);
            previewView_->setTemplateDatabase(nullptr);
            previewAttached_ = false;
        }
        previewView_->setProperty("spr.preview.serialized_resource", QVariant{});
        setStatus(
            QStringLiteral("Preview unavailable: %1").arg(QString::fromUtf8(exception.what())),
            true);
        updateSpriteAnimationTimer();
        return;
    }

    const std::vector<std::byte> payload = bytePayload(serialized);
    previewResources_->setPrimaryResourceOverride(
        kPreviewSpriteName,
        [payload]() -> std::optional<std::vector<std::byte>>
        {
            return payload;
        });

    const GlView::RecoveryState previousView = previewView_->recoveryState();
    previewView_->setProperty("spr.preview.geometry_mode", previewSprite.geometryMode);
    const ThingBox bounds = spritePreviewBox(previewSprite, context_.projectType);
    const bool refit = previewRefitPending_;
    bool updatedInPlace = false;
    if ( previewAttached_ && !refit && !materialChanged && pendingSpritePresentation_.isEmpty()
        && previewLevel_.kind == context_.projectType && previewLevel_.masterCMP == context_.masterCmp
        && previewLevel_.things.size() == 1u && previewLevel_.templates.size() == 1u )
    {
        updatedInPlace = previewView_->updateIsolatedSpritePreviewGeometry(previewSprite, bounds);
    }
    if ( updatedInPlace )
    {
        previewLevel_.things.front().bbox = bounds;
        previewLevel_.templates.front().bbox = bounds;
        (void)previewTemplates_.addOrUpdate(previewLevel_.templates.front(), true);
        previewView_->setProperty(
            "spr.preview.serialized_resource",
            QString::fromUtf8(serialized.data(), static_cast<qsizetype>(serialized.size())));
    }
    else
    {
        if ( previewAttached_ )
        {
            previewView_->setLevel(nullptr, GlView::SetLevelMode::keepView, nullptr);
            previewView_->setTemplateDatabase(nullptr);
            previewAttached_ = false;
        }

        previewLevel_.clear();
        previewTemplates_.clear();
        previewLevel_.kind = context_.projectType;
        previewLevel_.masterCMP = context_.masterCmp;
        previewLevel_.ppunit = 320.0;
        previewLevel_.layers = { "Preview" };
        previewLevel_.sectors.emplace_back();
        previewLevel_.sectors.back().layer = 0;
        // This isolated resource has no authored environment. A full-white
        // ambient floor masks every scalar/RGB extra-light edit.
        previewLevel_.sectors.back().ambient = ColorBlack;

        ThingTemplate previewTemplate{};
        previewTemplate.name = std::string(kPreviewTemplateName);
        previewTemplate.parent = "none";
        previewTemplate.bbox = bounds;
        TplValue spriteValue{};
        spriteValue.name = "sprite";
        spriteValue.type = ValueType::str;
        spriteValue.adjType = AdjType::spr;
        spriteValue.stringValue = std::string(kPreviewSpriteName);
        previewTemplate.values.push_back(std::move(spriteValue));
        TplValue typeValue{};
        typeValue.name = "type";
        typeValue.type = ValueType::str;
        typeValue.stringValue = "sprite";
        previewTemplate.values.push_back(std::move(typeValue));
        previewLevel_.templates.push_back(previewTemplate);
        (void)previewTemplates_.addOrUpdate(previewTemplate, true);

        Thing previewThing{};
        previewThing.name = std::string(kPreviewTemplateName);
        previewThing.templateName = std::string(kPreviewTemplateName);
        previewThing.sector = 0;
        previewThing.layer = 0;
        previewThing.bbox = bounds;
        previewLevel_.things.push_back(std::move(previewThing));

        previewView_->setResourceLocator(previewResources_.get());
        previewView_->setTemplateDatabase(&previewTemplates_);
        previewView_->setProperty(
            "resource_authoring.preview_cmp",
            QString::fromStdString(previewLevel_.getMasterCMP()));
        previewView_->setProperty(
            "spr.preview.serialized_resource",
            QString::fromUtf8(serialized.data(), static_cast<qsizetype>(serialized.size())));
        if ( !spritePreviewCels_.empty() )
        {
            const SpritePreviewMatCel& cel =
                spritePreviewCels_[static_cast<std::size_t>(
                    std::clamp(
                        spritePreviewCelIndex_,
                        0,
                        static_cast<int>(spritePreviewCels_.size()) - 1))];
            previewView_->primeMaterialTexture(
                sprite_.material,
                previewLevel_.getMasterCMP(),
                cel.image,
                cel.alphaBpp,
                cel.solidColor,
                cel.classicSolidLightRamp);
        }
        previewView_->setLevel(
            &previewLevel_,
            GlView::SetLevelMode::keepView,
            &previewTemplates_);
        previewAttached_ = true;
        // General level rebinding preserves the target by redistributing center
        // and pan. The isolated Sprite preview preserves both authored view
        // controls exactly, then only expands framing/depth for ordinary edits.
        previewView_->restoreRecoveryState(previousView, false);
    }
    previewRefitPending_ = false;
    previewView_->setShowThingVisualBounds(
        spritePreviewBoundsCheck_ && spritePreviewBoundsCheck_->isChecked());
    const bool wasUnavailable = previewView_->property("spr.preview.fit_unavailable").toBool();
    bool fitted = false;
    if ( pendingSpritePresentation_.isEmpty() )
    {
        fitted = previewView_->fitSpritePreviewToLevel(!refit);
    }
    else
    {
        applyPendingSpritePresentationAfterRefit();
        fitted = !previewView_->property("spr.preview.fit_unavailable").toBool();
    }
    previewView_->setProperty("spr.preview.fit_unavailable", !fitted);
    if ( !fitted )
    {
        setStatus(QStringLiteral("Sprite preview exceeds the camera's depth range."), true);
    }
    else if ( wasUnavailable )
    {
        setStatus(dirty_ ? QStringLiteral("Unsaved changes.") : QStringLiteral("Ready."));
    }
    if ( !updatedInPlace )
    {
        updateSpriteAnimationTimer();
    }
}

} // namespace sed::ui::resources::detail
