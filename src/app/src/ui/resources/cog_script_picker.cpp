#include "ui/resources/cog_script_picker.hpp"
#include "app_logging.hpp"
#include "ui/components/shared_ui_styles.hpp"
#include "ui/cog/cog_script_format_utils.hpp"

#include "sed/assets/resource_locator.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>
#include <vector>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QTextCursor>
#include <QUrl>

namespace sed::ui
{
namespace
{

QString normalizeCogName(const QString& name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }

    QString base = QFileInfo(trimmed).fileName();
    if (base.isEmpty())
    {
        base = trimmed;
    }

    if (!base.endsWith(".cog", Qt::CaseInsensitive))
    {
        base += ".cog";
    }
    return base;
}

enum class CogSourceKind : int
{
    all = 0,
    diskRoot = 1,
    gob = 2,
};

struct CogSource
{
    CogSourceKind kind{};
    int index{}; // root/gob index; -1 for all
    QString label{};
};

constexpr int kSourceKindRole = Qt::UserRole + 50;
constexpr int kSourceIndexRole = Qt::UserRole + 51;

// Legacy ResourcePicker behavior for COG: list only files directly under "cog/".
// Archive entries may use '\' separators, so normalize before matching.
std::optional<QString> tryExtractCogBasenameFromArchiveEntry(const std::string_view entryName)
{
    if (entryName.empty())
    {
        return std::nullopt;
    }

    std::string normalized{};
    normalized.reserve(entryName.size());
    for (const char ch : entryName)
    {
        normalized.push_back((ch == '\\') ? '/' : ch);
    }

    const std::string_view name = normalized;
    if (!sed::text::istartsWithAscii(name, "cog/") || !sed::text::iendsWithAscii(name, ".cog"))
    {
        return std::nullopt;
    }

    const std::string_view tail = name.substr(4);
    if (tail.empty() || tail.find('/') != std::string_view::npos)
    {
        return std::nullopt;
    }

    return QString::fromStdString(std::string(tail));
}

std::vector<std::filesystem::path> collectCogDiskRoots(
    const sed::assets::ResourceLocator& resources,
    const std::optional<std::filesystem::path>& preferredDiskRoot)
{
    std::vector<std::filesystem::path> out{};

    auto pathsEquivalent = [](const std::filesystem::path& a, const std::filesystem::path& b) -> bool
    {
        if (a.empty() || b.empty())
        {
            return false;
        }

        std::error_code ecA;
        std::error_code ecB;
        const bool aExists = std::filesystem::exists(a, ecA) && !ecA;
        const bool bExists = std::filesystem::exists(b, ecB) && !ecB;
        if (aExists && bExists)
        {
            std::error_code eqEc;
            if (std::filesystem::equivalent(a, b, eqEc) && !eqEc)
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
            while (key.endsWith('/'))
            {
                key.chop(1);
            }
            return key;
        };

        return normalize(a) == normalize(b);
    };

    auto addUnique = [&](const std::filesystem::path& p)
    {
        if (p.empty())
        {
            return;
        }
        for (const std::filesystem::path& existing : out)
        {
            if (pathsEquivalent(existing, p))
            {
                return;
            }
        }
        out.push_back(p);
    };

    if (preferredDiskRoot && !preferredDiskRoot->empty())
    {
        // Keep COG disk scan scope narrow: one canonical root (usually InstallDir/Resource).
        addUnique(*preferredDiskRoot);
    }
    else
    {
        for (const std::filesystem::path& root : resources.roots())
        {
            addUnique(root);
        }
    }

    return out;
}

std::vector<CogSource> collectCogSources(
    const sed::assets::ResourceLocator& resources,
    const std::optional<std::filesystem::path>& preferredDiskRoot)
{
    std::vector<CogSource> out{};
    out.push_back(CogSource{ CogSourceKind::all, -1, "All>COG" });

    // Mounted archives.
    for (std::size_t i = 0; i < resources.gobs().size(); ++i)
    {
        const sed::assets::GobArchive& gob = resources.gobs()[i];
        bool hasAny = false;
        for (const sed::assets::GobEntry& entry : gob.entries())
        {
            if (tryExtractCogBasenameFromArchiveEntry(entry.name))
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
        out.push_back(CogSource{
            CogSourceKind::gob,
            static_cast<int>(i),
            QString("%1>COG").arg(gobLabel) });
    }

    // Disk roots: scan only canonical root(s), and only `<root>/cog/`.
    const std::vector<std::filesystem::path> diskRoots = collectCogDiskRoots(resources, preferredDiskRoot);
    for (std::size_t i = 0; i < diskRoots.size(); ++i)
    {
        const std::filesystem::path& root = diskRoots[i];
        std::error_code ec;
        const std::filesystem::path cogDir = root / "cog";
        if (!std::filesystem::exists(cogDir, ec) || ec)
        {
            continue;
        }
        if (!std::filesystem::is_directory(cogDir, ec) || ec)
        {
            continue;
        }

        bool hasAny = false;
        for (const auto& de : std::filesystem::directory_iterator(cogDir, ec))
        {
            if (ec)
            {
                break;
            }
            if (de.is_regular_file(ec) && !ec)
            {
                const std::filesystem::path p = de.path();
                if (p.has_extension() && sed::text::iequalsAscii(p.extension().string(), ".cog"))
                {
                    hasAny = true;
                    break;
                }
            }
        }
        if (!hasAny)
        {
            continue;
        }

        QString rootLabel = QString::fromStdWString(root.filename().wstring());
        if (rootLabel.isEmpty())
        {
            rootLabel = QString::fromStdWString(root.wstring());
        }

        out.push_back(CogSource{
            CogSourceKind::diskRoot,
            static_cast<int>(i),
            QString("%1>COG").arg(rootLabel) });
    }

    return out;
}

std::vector<QString> collectScriptsForSource(
    const sed::assets::ResourceLocator& resources,
    const CogSourceKind kind,
    const int index,
    const std::optional<std::filesystem::path>& preferredDiskRoot)
{
    std::unordered_set<std::string> seen{};
    std::vector<QString> out{};

    auto addScript = [&](const QString& name)
    {
        const QString normalized = normalizeCogName(name);
        if (normalized.isEmpty())
        {
            return;
        }

        const std::string key = sed::text::upperAscii(normalized.toStdString());
        if (!seen.insert(key).second)
        {
            return;
        }

        out.push_back(normalized);
    };

    auto scanGob = [&](const sed::assets::GobArchive& gob)
    {
        for (const sed::assets::GobEntry& entry : gob.entries())
        {
            const std::optional<QString> baseName = tryExtractCogBasenameFromArchiveEntry(entry.name);
            if (!baseName)
            {
                continue;
            }

            addScript(*baseName);
        }
    };

    auto scanRoot = [&](const std::filesystem::path& root)
    {
        std::error_code ec;
        const std::filesystem::path cogDir = root / "cog";
        if (!std::filesystem::exists(cogDir, ec) || ec)
        {
            return;
        }
        if (!std::filesystem::is_directory(cogDir, ec) || ec)
        {
            return;
        }

        for (const auto& de : std::filesystem::directory_iterator(cogDir, ec))
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
            if (!fileName.endsWith(".cog", Qt::CaseInsensitive))
            {
                continue;
            }

            addScript(fileName);
        }
    };

    if (kind == CogSourceKind::all || kind == CogSourceKind::gob)
    {
        if (kind == CogSourceKind::gob)
        {
            if (index >= 0 && static_cast<std::size_t>(index) < resources.gobs().size())
            {
                scanGob(resources.gobs()[static_cast<std::size_t>(index)]);
            }
        }
        else
        {
            for (const sed::assets::GobArchive& gob : resources.gobs())
            {
                scanGob(gob);
            }
        }
    }

    if (kind == CogSourceKind::all || kind == CogSourceKind::diskRoot)
    {
        const std::vector<std::filesystem::path> diskRoots = collectCogDiskRoots(resources, preferredDiskRoot);
        if (kind == CogSourceKind::diskRoot)
        {
            if (index >= 0 && static_cast<std::size_t>(index) < diskRoots.size())
            {
                scanRoot(diskRoots[static_cast<std::size_t>(index)]);
            }
        }
        else
        {
            for (const std::filesystem::path& root : diskRoots)
            {
                scanRoot(root);
            }
        }
    }

    std::sort(out.begin(), out.end(), [](const QString& a, const QString& b)
        {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });

    return out;
}

std::optional<std::string> tryReadCogScriptTextFromDiskRoot(
    const std::filesystem::path& root,
    const QString& scriptName)
{
    const std::filesystem::path path = root / "cog" / std::filesystem::path(scriptName.toStdWString());
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return std::nullopt;
    }

    std::string text;
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (size > 0)
    {
        text.resize(static_cast<std::size_t>(size));
        in.read(text.data(), static_cast<std::streamsize>(text.size()));
    }
    else
    {
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    return text;
}

std::optional<std::string> tryReadCogScriptTextFromGob(
    const sed::assets::GobArchive& gob,
    const QString& scriptName)
{
    const std::string entry = "cog/" + scriptName.toStdString();
    if (!gob.contains(entry))
    {
        return std::nullopt;
    }

    try
    {
        const std::vector<std::byte> data = gob.read(entry);
        return std::string(reinterpret_cast<const char*>(data.data()), data.size());
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void showLoggedCogPickerFailure(
    QWidget* parent,
    const QString& title,
    const QString& message,
    const QString& logRecord)
{
    qCWarning(logResources).noquote() << logRecord;

    QMessageBox dialog(
        QMessageBox::Information,
        title,
        message,
        QMessageBox::Ok,
        parent);
    dialog.setProperty(kMessageBoxLoggingSkipProperty, true);
    (void)dialog.exec();
}

std::optional<std::filesystem::path> writeCogTemplate(
    QWidget* parent,
    const std::filesystem::path& extractionRoot,
    const QString& baseName,
    const bool openAfterCreate)
{
    QString base = normalizeCogName(baseName);
    if (base.isEmpty())
    {
        return std::nullopt;
    }

    const std::filesystem::path outDir = extractionRoot / "cog";
    std::error_code mkEc;
    std::filesystem::create_directories(outDir, mkEc);
    if (mkEc)
    {
        showLoggedCogPickerFailure(
            parent,
            QStringLiteral("Create script"),
            QStringLiteral("Couldn't write new script to disk."),
            QStringLiteral(
                "COG script creation failed: phase=create output directory; "
                "path=%1; reason=%2")
                .arg(
                    QDir::toNativeSeparators(
                        QString::fromStdWString(outDir.wstring())),
                    QString::fromStdString(mkEc.message())));
        return std::nullopt;
    }

    const std::filesystem::path outPath = outDir / std::filesystem::path(base.toStdWString());
    std::error_code ec;
    const bool exists = std::filesystem::exists(outPath, ec) && !ec;
    if (exists)
    {
        QMessageBox confirmation(
            QMessageBox::Question,
            QStringLiteral("Create script"),
            QString("Script already exists:\n%1\n\nOverwrite it?")
                .arg(QString::fromStdWString(outPath.wstring())),
            QMessageBox::Yes | QMessageBox::No,
            parent,
            Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
        confirmation.setDefaultButton(QMessageBox::No);
        confirmation.setEscapeButton(QMessageBox::No);
        confirmation.setProperty(
            kMessageBoxLoggingDestructiveProperty, true);
        confirmation.setProperty(kMessageBoxLoggingContextProperty,
            QStringLiteral("COG path: %1").arg(
                QDir::toNativeSeparators(
                    QString::fromStdWString(outPath.wstring()))));
        if (confirmation.exec() != QMessageBox::Yes)
        {
            return std::nullopt;
        }
    }

    const QByteArray templateBytes = cog::skeletonSnippet().toUtf8();

    std::ofstream out(outPath, std::ios::binary);
    if (!out)
    {
        showLoggedCogPickerFailure(
            parent,
            QStringLiteral("Create script"),
            QStringLiteral("Couldn't write new script to disk."),
            QStringLiteral(
                "COG script creation failed: phase=open output; path=%1; "
                "reason=output file could not be opened")
                .arg(QDir::toNativeSeparators(
                    QString::fromStdWString(outPath.wstring()))));
        return std::nullopt;
    }
    out.write(templateBytes.constData(), static_cast<std::streamsize>(templateBytes.size()));
    out.close();
    if (!out)
    {
        showLoggedCogPickerFailure(
            parent,
            QStringLiteral("Create script"),
            QStringLiteral("Couldn't write new script to disk."),
            QStringLiteral(
                "COG script creation failed: phase=write output; path=%1; "
                "reason=script data could not be written completely")
                .arg(QDir::toNativeSeparators(
                    QString::fromStdWString(outPath.wstring()))));
        return std::nullopt;
    }

    if (openAfterCreate)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(outPath.wstring())));
    }

    return outPath;
}

class CogScriptPickerDialog final : public QDialog
{
public:
    CogScriptPickerDialog(
        QWidget* parent,
        const sed::assets::ResourceLocator& resources,
        const std::optional<std::filesystem::path>& extractionRoot)
        : QDialog(parent)
        , resources_(resources)
        , extractionRoot_(extractionRoot)
    {
        ensureGlobalComponentStyles();
        setWindowTitle("Resource Picker");
        setModal(true);
        // Match (roughly) the original SED dialog proportions (RESOURCEPICKER.DFM).
        resize(1464, 730);
        setMinimumSize(900, 600);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(8);

        auto* content = new QSplitter(Qt::Horizontal, this);
        content->setChildrenCollapsible(false);
        root->addWidget(content, 1);

        auto* left = new QSplitter(Qt::Vertical, content);
        left->setChildrenCollapsible(false);
        content->addWidget(left);

        sources_ = new QTreeWidget(left);
        sources_->setHeaderHidden(true);
        sources_->setRootIsDecorated(false);
        sources_->setUniformRowHeights(true);
        sources_->setSelectionMode(QAbstractItemView::SingleSelection);
        sources_->setSelectionBehavior(QAbstractItemView::SelectRows);
        sources_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        sources_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        sources_->setIndentation(0);
        sources_->viewport()->installEventFilter(this);
        left->addWidget(sources_);

        preview_ = new QPlainTextEdit(left);
        preview_->setReadOnly(true);
        preview_->setLineWrapMode(QPlainTextEdit::NoWrap);
        left->addWidget(preview_);

        scripts_ = new QListWidget(content);
        scripts_->setSelectionMode(QAbstractItemView::SingleSelection);
        scripts_->setViewMode(QListView::ListMode);
        scripts_->setFlow(QListView::TopToBottom); // wrap into columns
        scripts_->setWrapping(true);
        scripts_->setResizeMode(QListView::Adjust);
        scripts_->setMovement(QListView::Static);
        scripts_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        scripts_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        scripts_->setUniformItemSizes(true);
        scripts_->setWordWrap(false);
        scripts_->setSpacing(0);
        scripts_->viewport()->installEventFilter(this);
        content->addWidget(scripts_);

        if (QScrollBar* scroll = sources_->verticalScrollBar())
        {
            scroll->setSingleStep(32);
        }
        if (QScrollBar* scroll = scripts_->verticalScrollBar())
        {
            scroll->setSingleStep(32);
        }
        if (QScrollBar* scroll = scripts_->horizontalScrollBar())
        {
            scroll->setSingleStep(32);
        }

        content->setStretchFactor(0, 0);
        content->setStretchFactor(1, 1);

        createPane_ = new QWidget(this);
        auto* createLayout = new QHBoxLayout(createPane_);
        createLayout->setContentsMargins(0, 0, 0, 0);
        createLayout->setSpacing(6);

        auto* newLabel = new QLabel("New:", createPane_);
        createLayout->addWidget(newLabel);

        createName_ = new QLineEdit(createPane_);
        createName_->setPlaceholderText("new.cog");
        createLayout->addWidget(createName_, 1);

        openAfterCreate_ = new QCheckBox("Open", createPane_);
        openAfterCreate_->setChecked(true);
        openAfterCreate_->setToolTip("Open the new script in your default editor after creating it");
        createLayout->addWidget(openAfterCreate_);

        createBtn_ = new QPushButton("Create", createPane_);
        createLayout->addWidget(createBtn_);

        cancelCreateBtn_ = new QPushButton("Cancel", createPane_);
        createLayout->addWidget(cancelCreateBtn_);

        createPane_->setVisible(false);
        root->addWidget(createPane_);

        auto* bottom = new QWidget(this);
        auto* bottomLayout = new QHBoxLayout(bottom);
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(6);

        auto* maskLabel = new QLabel("Mask:", bottom);
        bottomLayout->addWidget(maskLabel);

        filter_ = new QLineEdit(bottom);
        filter_->setPlaceholderText("Search...");
        filter_->setFixedWidth(180);
        bottomLayout->addWidget(filter_);

        selected_ = new QLineEdit(bottom);
        selected_->setPlaceholderText("script.cog");
        bottomLayout->addWidget(selected_, 1);

        bottomLayout->addStretch(1);

        buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, bottom);
        bottomLayout->addWidget(buttons_);

        root->addWidget(bottom);

        okButton_ = buttons_->button(QDialogButtonBox::Ok);
        if (okButton_)
        {
            okButton_->setEnabled(false);
        }

        connect(filter_, &QLineEdit::textChanged, this, [this]()
            {
                rebuildScriptList();
            });

        connect(selected_, &QLineEdit::textChanged, this, [this](const QString&)
            {
                updateOkState();
            });
        connect(selected_, &QLineEdit::returnPressed, this, [this]()
            {
                const QString want = normalizeCogName(selected_ ? selected_->text() : QString{});
                if (want.isEmpty())
                {
                    return;
                }

                for (int i = 0; i < scripts_->count(); ++i)
                {
                    if (auto* it = scripts_->item(i))
                    {
                        if (it->text().compare(want, Qt::CaseInsensitive) == 0)
                        {
                            scripts_->setCurrentItem(it);
                            break;
                        }
                    }
                }
            });

        connect(sources_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*)
            {
                refreshScriptsForSource();
            });

        connect(scripts_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem*, QListWidgetItem*)
            {
                const QString s = currentScript();
                if (selected_)
                {
                    QSignalBlocker b(selected_);
                    selected_->setText(s);
                }
                updateOkState();
                updatePreview();
            });

        connect(scripts_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*)
            {
                accept();
            });

        connect(cancelCreateBtn_, &QPushButton::clicked, this, [this]()
            {
                if (createPane_)
                {
                    createPane_->setVisible(false);
                }
            });

        connect(createBtn_, &QPushButton::clicked, this, [this]()
            {
                if (!extractionRoot_)
                {
                    const QString requestedScript = normalizeCogName(
                        createName_ ? createName_->text() : QString{});
                    showLoggedCogPickerFailure(
                        this,
                        QStringLiteral("Create script"),
                        QStringLiteral(
                            "No resource root is configured; can't create script."),
                        QStringLiteral(
                            "COG script creation failed: phase=resolve output "
                            "root; script=%1; reason=no resource root is "
                            "configured")
                            .arg(requestedScript.isEmpty()
                                ? QStringLiteral("<unnamed>")
                                : requestedScript));
                    return;
                }
                const QString base = createName_ ? createName_->text() : QString{};
                const bool openNow = openAfterCreate_ ? openAfterCreate_->isChecked() : false;
                const auto out = writeCogTemplate(this, *extractionRoot_, base, openNow);
                if (!out)
                {
                    return;
                }

                createdNew_ = true;
                const QString newName = normalizeCogName(base);
                if (!newName.isEmpty())
                {
                    rebuildSources();
                    refreshScriptsForSource();
                    selectScript(newName);
                }

                if (createPane_)
                {
                    createPane_->setVisible(false);
                }
            });

        connect(buttons_, &QDialogButtonBox::accepted, this, [this]()
            {
                accept();
            });
        connect(buttons_, &QDialogButtonBox::rejected, this, [this]()
            {
                reject();
            });

        rebuildSources();
        refreshScriptsForSource();
    }

    std::optional<CogScriptPickerResult> result() const
    {
        if (resultScript_.empty())
        {
            return std::nullopt;
        }

        CogScriptPickerResult out{};
        out.scriptName = resultScript_;
        out.createdNew = createdNew_;
        return out;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        auto handleShiftWheelPageScroll = [&](QAbstractItemView* view) -> bool
        {
            if (!view || !event || event->type() != QEvent::Wheel)
            {
                return false;
            }

            auto* wheelEvent = static_cast<QWheelEvent*>(event);
            if ((wheelEvent->modifiers() & Qt::ShiftModifier) == 0)
            {
                return false;
            }

            if (QScrollBar* scroll = view->verticalScrollBar())
            {
                const QPoint delta = wheelEvent->angleDelta();
                const int dominantDelta = (std::abs(delta.y()) >= std::abs(delta.x()))
                    ? delta.y()
                    : delta.x();
                if (dominantDelta != 0)
                {
                    const int direction = dominantDelta < 0 ? 1 : -1;
                    scroll->setValue(scroll->value() + direction * std::max(1, scroll->pageStep()));
                }
            }
            wheelEvent->accept();
            return true;
        };

        if (sources_ && watched == sources_->viewport() && handleShiftWheelPageScroll(sources_))
        {
            return true;
        }
        if (scripts_ && watched == scripts_->viewport() && handleShiftWheelPageScroll(scripts_))
        {
            return true;
        }

        return QDialog::eventFilter(watched, event);
    }

    void accept() override
    {
        QString selected = currentScript();
        if (selected.isEmpty() && selected_)
        {
            selected = normalizeCogName(selected_->text());
        }

        selected = normalizeCogName(selected);
        if (selected.isEmpty())
        {
            return;
        }

        const std::string scriptName = selected.toStdString();
        if (!resources_.tryReadTextWithExt(scriptName, ".cog"))
        {
            showLoggedCogPickerFailure(
                this,
                QStringLiteral("Pick script"),
                QString("Script not found in resources:\n%1").arg(selected),
                QStringLiteral(
                    "COG script selection failed: phase=read selected "
                    "resource; script=%1; reason=resource is missing or "
                    "unreadable")
                    .arg(selected));
            return;
        }

        resultScript_ = scriptName;
        QDialog::accept();
    }

private:
    QString currentScript() const
    {
        if (!scripts_)
        {
            return {};
        }
        const auto* item = scripts_->currentItem();
        return item ? item->text().trimmed() : QString{};
    }

    void rebuildSources()
    {
        if (!sources_)
        {
            return;
        }

        const int prevKind = sources_->currentItem() ? sources_->currentItem()->data(0, kSourceKindRole).toInt() : static_cast<int>(CogSourceKind::all);
        const int prevIndex = sources_->currentItem() ? sources_->currentItem()->data(0, kSourceIndexRole).toInt() : -1;

        sources_->clear();
        sourcesData_ = collectCogSources(resources_, extractionRoot_);

        QTreeWidgetItem* best = nullptr;
        for (const CogSource& s : sourcesData_)
        {
            auto* item = new QTreeWidgetItem(sources_);
            item->setText(0, s.label);
            item->setData(0, kSourceKindRole, static_cast<int>(s.kind));
            item->setData(0, kSourceIndexRole, s.index);

            if (static_cast<int>(s.kind) == prevKind && s.index == prevIndex)
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

    void refreshScriptsForSource()
    {
        const CogSourceKind kind = currentSourceKind();
        const int idx = currentSourceIndex();
        scriptsAll_ = collectScriptsForSource(resources_, kind, idx, extractionRoot_);
        rebuildScriptList();
    }

    void rebuildScriptList()
    {
        if (!scripts_)
        {
            return;
        }

        const QString prev = currentScript();

        scripts_->clear();

        const QString mask = filter_ ? filter_->text().trimmed() : QString{};
        const bool hasMask = !mask.isEmpty();

        QFontMetrics fm(scripts_->font());
        int maxTextW = 0;
        const int rowH = std::max(16, fm.height() + 4);

        for (const QString& s : scriptsAll_)
        {
            if (hasMask && !s.contains(mask, Qt::CaseInsensitive))
            {
                continue;
            }

            auto* item = new QListWidgetItem(s, scripts_);
            if (resources_.findOnDisk(s.toStdString()).has_value())
            {
                QFont fnt = item->font();
                fnt.setBold(true);
                item->setFont(fnt);
            }

            maxTextW = std::max(maxTextW, fm.horizontalAdvance(s));
        }

        const int colW = std::clamp(maxTextW + 22, 170, 360);
        scripts_->setGridSize(QSize(colW, rowH));

        selectScript(prev);
        updateOkState();
        updatePreview();
    }

    void selectScript(const QString& script)
    {
        if (!scripts_ || script.isEmpty())
        {
            return;
        }

        for (int i = 0; i < scripts_->count(); ++i)
        {
            if (auto* it = scripts_->item(i))
            {
                if (it->text().compare(script, Qt::CaseInsensitive) == 0)
                {
                    scripts_->setCurrentItem(it);
                    return;
                }
            }
        }
    }

    void updateOkState()
    {
        const QString cur = normalizeCogName(selected_ ? selected_->text() : currentScript());
        const bool have = !cur.isEmpty();
        if (okButton_)
        {
            okButton_->setEnabled(have);
        }

    }

    CogSourceKind currentSourceKind() const
    {
        if (!sources_ || !sources_->currentItem())
        {
            return CogSourceKind::all;
        }
        return static_cast<CogSourceKind>(sources_->currentItem()->data(0, kSourceKindRole).toInt());
    }

    int currentSourceIndex() const
    {
        if (!sources_ || !sources_->currentItem())
        {
            return -1;
        }
        return sources_->currentItem()->data(0, kSourceIndexRole).toInt();
    }

    void updatePreview()
    {
        if (!preview_)
        {
            return;
        }

        const QString script = currentScript();
        if (script.isEmpty())
        {
            preview_->clear();
            return;
        }

        const CogSourceKind kind = currentSourceKind();
        const int idx = currentSourceIndex();

        std::optional<std::string> text{};
        if (kind == CogSourceKind::diskRoot)
        {
            const std::vector<std::filesystem::path> diskRoots = collectCogDiskRoots(resources_, extractionRoot_);
            if (idx >= 0 && static_cast<std::size_t>(idx) < diskRoots.size())
            {
                text = tryReadCogScriptTextFromDiskRoot(diskRoots[static_cast<std::size_t>(idx)], script);
            }
        }
        else if (kind == CogSourceKind::gob)
        {
            if (idx >= 0 && static_cast<std::size_t>(idx) < resources_.gobs().size())
            {
                text = tryReadCogScriptTextFromGob(resources_.gobs()[static_cast<std::size_t>(idx)], script);
            }
        }
        else
        {
            text = resources_.tryReadTextWithExt(script.toStdString(), ".cog");
        }

        if (!text)
        {
            preview_->setPlainText(QString("Couldn't load:\n%1").arg(script));
            return;
        }

        preview_->setPlainText(QString::fromStdString(*text));
        preview_->moveCursor(QTextCursor::Start);
    }

    const sed::assets::ResourceLocator& resources_;
    std::optional<std::filesystem::path> extractionRoot_{};

    QLineEdit* filter_{};
    QLineEdit* selected_{};
    QTreeWidget* sources_{};
    QPlainTextEdit* preview_{};
    QListWidget* scripts_{};
    QWidget* createPane_{};
    QLineEdit* createName_{};
    QCheckBox* openAfterCreate_{};
    QPushButton* createBtn_{};
    QPushButton* cancelCreateBtn_{};
    QDialogButtonBox* buttons_{};
    QPushButton* okButton_{};

    std::vector<CogSource> sourcesData_{};
    std::vector<QString> scriptsAll_{};
    std::string resultScript_{};
    bool createdNew_{};
};

} // namespace

std::optional<CogScriptPickerResult> pickCogScript(
    QWidget* parent,
    const sed::assets::ResourceLocator& resources,
    const std::optional<std::filesystem::path>& extractionRoot)
{
    CogScriptPickerDialog dlg(parent, resources, extractionRoot);
    if (dlg.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }
    return dlg.result();
}

} // namespace sed::ui
