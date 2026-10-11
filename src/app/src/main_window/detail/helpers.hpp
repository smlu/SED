#pragma once

namespace
{

QIcon makeFlatFolderIcon(const QColor& color, const int logicalSize, const qreal devicePixelRatio)
{
    const int s = std::max(8, logicalSize);
    const qreal dpr = (devicePixelRatio > 0.0) ? devicePixelRatio : 1.0;
    const int ds = std::max(1, static_cast<int>(std::lround(static_cast<qreal>(s) * dpr)));

    QPixmap pixmap(ds, ds);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(dpr);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    const int w = ds;
    const int h = ds;

    const QRect body(w * 2 / 16, h * 6 / 16, w * 8 / 16, h * 4 / 16);
    const QRect bodyfrac(w * 2 / 16, h * 3 / 16, w * 8 / 16, h * 2 / 16);
    const QRect tab(w * 2 / 16, h * 2 / 16, w * 3 / 16, h * 2 / 16);

    p.fillRect(body, color);
    p.fillRect(bodyfrac, color);
    p.fillRect(tab, color);
    p.end();

    return QIcon(pixmap);
}

constexpr int kMaxRecentLevels = 15;
constexpr auto kSelectionHintVisibleProperty = "sedSelectionHintVisible";
constexpr auto kSelectionHintTextProperty = "sedSelectionHintText";

bool recentLevelPathsMatch(const QString& lhs, const QString& rhs)
{
    return QString::compare(lhs.trimmed(), rhs.trimmed(), Qt::CaseInsensitive) == 0;
}

bool recentLevelListContainsPathLocal(const QStringList& paths, const QString& needle)
{
    return std::any_of(
        paths.begin(),
        paths.end(),
        [&needle](const QString& path)
        {
            return recentLevelPathsMatch(path, needle);
        });
}

void removeRecentLevelPathLocal(QStringList& paths, const QString& needle)
{
    for (int i = paths.size() - 1; i >= 0; --i)
    {
        if (recentLevelPathsMatch(paths[i], needle))
        {
            paths.removeAt(i);
        }
    }
}

/**
 * Records a level path in last-used recent order while preserving pinned entries
 * when the list needs to be trimmed.
 */
void rememberRecentLevelFile(const QString& filename)
{
    const QFileInfo info(filename);
    const QString absolutePath = info.absoluteFilePath();
    if (absolutePath.trimmed().isEmpty())
    {
        return;
    }

    QStringList recent = loadRecentLevels();
    removeRecentLevelPathLocal(recent, absolutePath);
    recent.prepend(absolutePath);

    const QStringList pinned = loadPinnedRecentLevels();
    while (recent.size() > kMaxRecentLevels)
    {
        int removeIndex = recent.size() - 1;
        while (removeIndex > 0 && recentLevelListContainsPathLocal(pinned, recent[removeIndex]))
        {
            --removeIndex;
        }

        if (removeIndex == 0)
        {
            removeIndex = recent.size() - 1;
        }

        recent.removeAt(removeIndex);
    }

    saveRecentLevels(recent);
}

void forgetRecentLevelFile(const QString& filename)
{
    const QString trimmed = filename.trimmed();
    if (trimmed.isEmpty())
    {
        return;
    }

    QStringList recent = loadRecentLevels();
    removeRecentLevelPathLocal(recent, trimmed);
    saveRecentLevels(recent);

    QStringList pinned = loadPinnedRecentLevels();
    removeRecentLevelPathLocal(pinned, trimmed);
    savePinnedRecentLevels(pinned);
}

/**
 * Returns an action shortcut list for utility functions that intentionally do
 * not depend on QAction. Null actions map to an empty binding list.
 */
QList<QKeySequence> shortcutBindingsForAction(const QAction* action)
{
    return action ? action->shortcuts() : QList<QKeySequence>{};
}

std::optional<ProjectType> readLastProjectKindSetting() noexcept
{
    QSettings settings = settingsStore();
    return ui::resources::parseProjectTypeSettingValue(settings.value("app/last_project_kind").toString());
}

void writeLastProjectKindSetting(const ProjectType kind) noexcept
{
    const std::optional<QString> projectNameOpt = ui::resources::settingsProjectName(kind);
    if (!projectNameOpt)
    {
        return;
    }

    QSettings settings = settingsStore();
    settings.setValue("app/last_project_kind", *projectNameOpt);
}

std::optional<std::filesystem::path> readProjectInstallDirSetting(const ProjectType kind)
{
    QSettings settings = settingsStore();
    const std::optional<QString> projectNameOpt = ui::resources::settingsProjectName(kind);
    if (!projectNameOpt)
    {
        return std::nullopt;
    }

    const QString key = QString("resources/%1/install_dir").arg(*projectNameOpt);
    const QString value = settings.value(key).toString().trimmed();
    if (value.isEmpty())
    {
        return std::nullopt;
    }

    return std::filesystem::path(value.toStdWString());
}

void writeProjectInstallDirSetting(const ProjectType kind, const std::optional<std::filesystem::path>& installDir)
{
    QSettings settings = settingsStore();
    const std::optional<QString> projectNameOpt = ui::resources::settingsProjectName(kind);
    if (!projectNameOpt)
    {
        return;
    }

    const QString key = QString("resources/%1/install_dir").arg(*projectNameOpt);
    if (!installDir || installDir->empty())
    {
        settings.remove(key);
        return;
    }

    std::error_code ec;
    const std::filesystem::path abs = std::filesystem::absolute(*installDir, ec).lexically_normal();
    settings.setValue(key, QString::fromStdWString((!ec ? abs : *installDir).wstring()));
}

void maybeMigrateLegacyResourceSettings()
{
    static bool migrated = false;
    if (migrated)
    {
        return;
    }
    migrated = true;

    QSettings settings = settingsStore();

    const QString legacyRootsKey = "resources/roots";
    const QString legacyGobsKey = "resources/gobs";
    const QStringList legacyRoots = settings.value(legacyRootsKey).toStringList();
    const QStringList legacyGobs = settings.value(legacyGobsKey).toStringList();
    if (legacyRoots.isEmpty() && legacyGobs.isEmpty())
    {
        return;
    }

    QStringList jkdf2Roots;
    QStringList motsRoots;
    QStringList ijimRoots;

    QStringList jkdf2Gobs;
    QStringList motsGobs;
    QStringList ijimGobs;

    const auto fileExists = [](const std::filesystem::path& path) -> bool
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec) && !ec;
    };

    const auto looksLikeProjectRoot = [&](const std::filesystem::path& root, const ProjectType kind) -> bool
    {
        const QString rootLower = QString::fromStdWString(root.wstring()).toLower();

        if (kind == ProjectType::IJIM)
        {
            if (rootLower.contains("infernal machine") || rootLower.contains("ijim"))
            {
                return true;
            }

            const std::array<std::filesystem::path, 4> candidates{
                root / "CD1.GOB",
                root / "CD2.GOB",
                root / "Resource" / "CD1.GOB",
                root / "Resource" / "CD2.GOB",
            };
            return std::any_of(candidates.begin(), candidates.end(), fileExists);
        }

        if (kind == ProjectType::MOTS)
        {
            if (rootLower.contains("mysteries of the sith") || rootLower.contains("mots") || rootLower.contains("jkm"))
            {
                return true;
            }

            const std::array<std::filesystem::path, 6> candidates{
                root / "Jkmres.goo",
                root / "JKMsndLO.goo",
                root / "Jkm.goo",
                root / "Resource" / "Jkmres.goo",
                root / "Resource" / "JKMsndLO.goo",
                root / "Resource" / "Jkm.goo",
            };
            return std::any_of(candidates.begin(), candidates.end(), fileExists);
        }

        if (kind == ProjectType::JKDF2)
        {
            if (rootLower.contains("dark forces") || rootLower.contains("jkdf2") || rootLower.contains("jediknight"))
            {
                return true;
            }

            const std::array<std::filesystem::path, 8> candidates{
                root / "Res2.gob",
                root / "Res1hi.gob",
                root / "Res1low.gob",
                root / "Jk1.gob",
                root / "Resource" / "Res2.gob",
                root / "Resource" / "Res1hi.gob",
                root / "Resource" / "Res1low.gob",
                root / "Episode" / "Res2.gob",
            };
            return std::any_of(candidates.begin(), candidates.end(), fileExists);
        }

        return false;
    };

    for (const QString& root : legacyRoots)
    {
        const QString trimmed = root.trimmed();
        if (trimmed.isEmpty())
        {
            continue;
        }

        const std::filesystem::path path(trimmed.toStdWString());

        if (looksLikeProjectRoot(path, ProjectType::IJIM))
        {
            ijimRoots.push_back(trimmed);
            continue;
        }

        if (looksLikeProjectRoot(path, ProjectType::MOTS))
        {
            motsRoots.push_back(trimmed);
            continue;
        }

        if (looksLikeProjectRoot(path, ProjectType::JKDF2))
        {
            jkdf2Roots.push_back(trimmed);
            continue;
        }
    }

    for (const QString& gob : legacyGobs)
    {
        const QString trimmed = gob.trimmed();
        if (trimmed.isEmpty())
        {
            continue;
        }

        const std::filesystem::path path(trimmed.toStdWString());
        const std::string ext = sed::text::lowerAscii(path.extension().string());

        if (ext == ".gob")
        {
            const std::string base = sed::text::lowerAscii(path.filename().string());

            if (base == "cd1.gob" || base == "cd2.gob")
            {
                ijimGobs.push_back(trimmed);
            }
            else
            {
                jkdf2Gobs.push_back(trimmed);
            }
        }
        else if (ext == ".goo")
        {
            motsGobs.push_back(trimmed);
        }
    }

    const auto setIfEmpty = [&](const QString& key, const QStringList& value)
    {
        if (!settings.value(key).toStringList().isEmpty())
        {
            return;
        }
        if (value.isEmpty())
        {
            return;
        }
        settings.setValue(key, value);
    };

    setIfEmpty("resources/jkdf2/gobs", jkdf2Gobs);
    setIfEmpty("resources/mots/gobs", motsGobs);
    setIfEmpty("resources/ijim/gobs", ijimGobs);

    setIfEmpty("resources/jkdf2/roots", jkdf2Roots);
    setIfEmpty("resources/mots/roots", motsRoots);
    setIfEmpty("resources/ijim/roots", ijimRoots);

    settings.remove(legacyRootsKey);
    settings.remove(legacyGobsKey);
}

std::optional<std::filesystem::path> tryGetProjectInstallDir(const ProjectType kind)
{
    if (const auto configured = readProjectInstallDirSetting(kind))
    {
        std::error_code ec;
        if (std::filesystem::exists(*configured, ec) && !ec
            && ui::resources::validateProjectInstallPath(kind, *configured)
                .canUse())
        {
            return configured;
        }
    }

    auto readEnv = [](const char* name) -> std::optional<std::filesystem::path>
    {
        const QByteArray value = qgetenv(name);
        if (value.isEmpty())
        {
            return std::nullopt;
        }

        const QString trimmed = QString::fromUtf8(value).trimmed();
        if (trimmed.isEmpty())
        {
            return std::nullopt;
        }

        return std::filesystem::path(trimmed.toStdWString());
    };

    switch (kind)
    {
    case ProjectType::JKDF2:
        if (const auto v = readEnv("SED_JKDF2_DIR"))
        {
            return v;
        }
        break;
    case ProjectType::MOTS:
        if (const auto v = readEnv("SED_MOTS_DIR"))
        {
            return v;
        }
        break;
    case ProjectType::IJIM:
        if (const auto v = readEnv("SED_IJIM_DIR"))
        {
            return v;
        }
        break;
    default:
        break;
    }

#if defined(Q_OS_WIN)
    auto readInstallPath = [](const QString& subKey) -> std::optional<std::filesystem::path>
    {
        static constexpr std::string_view kValueName = "Install Path";
        const QString wowKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\") + subKey;
        const QString nativeKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\") + subKey;
        const std::array<QString, 2> keys{ wowKey, nativeKey };

        for (const QString& key : keys)
        {
            QSettings reg(key, QSettings::NativeFormat);
            const QString installPath = reg.value(QString::fromUtf8(kValueName.data(), static_cast<int>(kValueName.size()))).toString().trimmed();
            if (!installPath.isEmpty())
            {
                return std::filesystem::path(installPath.toStdWString());
            }
        }

        return std::nullopt;
    };

    std::optional<std::filesystem::path> regInstall{};
    switch (kind)
    {
    case ProjectType::JKDF2:
        regInstall = readInstallPath(QStringLiteral("LucasArts Entertainment Company\\JediKnight\\v1.0"));
        break;
    case ProjectType::MOTS:
        regInstall = readInstallPath(QStringLiteral("LucasArts Entertainment Company LLC\\Mysteries of the Sith\\v1.0"));
        break;
    case ProjectType::IJIM:
        regInstall = readInstallPath(QStringLiteral("LucasArts Entertainment Company LLC\\Indiana Jones and the Infernal Machine\\v1.0"));
        break;
    default:
        break;
    }

    if (regInstall)
    {
        return regInstall;
    }
#else
    (void)kind;
#endif

    const auto fileExists = [](const std::filesystem::path& path) -> bool
    {
        std::error_code ec;
        return std::filesystem::exists(path, ec) && !ec;
    };

    const auto looksLikeInstallDir = [&](const std::filesystem::path& dir) -> bool
    {
        switch (kind)
        {
        case ProjectType::IJIM:
        {
            const std::array<std::filesystem::path, 4> candidates{
                dir / "Resource" / "CD1.GOB",
                dir / "Resource" / "CD2.GOB",
                dir / "CD1.GOB",
                dir / "CD2.GOB",
            };
            return std::any_of(candidates.begin(), candidates.end(), fileExists);
        }
        case ProjectType::JKDF2:
        {
            const std::array<std::filesystem::path, 8> candidates{
                dir / "Resource" / "Res2.gob",
                dir / "Episode" / "Res2.gob",
                dir / "Res2.gob",
                dir / "Resource" / "Res1hi.gob",
                dir / "Resource" / "Res1low.gob",
                dir / "Res1hi.gob",
                dir / "Res1low.gob",
                dir / "Jk1.gob",
            };
            return std::any_of(candidates.begin(), candidates.end(), fileExists);
        }
        case ProjectType::MOTS:
        {
            const std::array<std::filesystem::path, 6> candidates{
                dir / "Resource" / "Jkmres.goo",
                dir / "Resource" / "JKMsndLO.goo",
                dir / "Resource" / "Jkm.goo",
                dir / "Jkmres.goo",
                dir / "JKMsndLO.goo",
                dir / "Jkm.goo",
            };
            return std::any_of(candidates.begin(), candidates.end(), fileExists);
        }
        default:
            return false;
        }
    };

    const auto findDevInstallDir = [&](std::filesystem::path base) -> std::optional<std::filesystem::path>
    {
        for (int depth = 0; depth < 8; ++depth)
        {
            if (base.empty())
            {
                break;
            }

            if (looksLikeInstallDir(base))
            {
                return base;
            }

            const std::filesystem::path gameDir = base / "game";
            if (looksLikeInstallDir(gameDir))
            {
                return gameDir;
            }

            const std::filesystem::path parent = base.parent_path();
            if (parent == base)
            {
                break;
            }
            base = parent;
        }

        return std::nullopt;
    };

    try
    {
        if (const auto found = findDevInstallDir(std::filesystem::current_path()))
        {
            return found;
        }
    }
    catch (...)
    {
    }

    try
    {
        const std::filesystem::path exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
        if (const auto found = findDevInstallDir(exeDir))
        {
            return found;
        }
    }
    catch (...)
    {
    }

    return std::nullopt;
}

ui::resources::ResourceSettings readResourceSettings(
    const ProjectType kind,
    const bool allowLegacyFallback,
    const bool migrateLegacy = true)
{
    if (migrateLegacy)
    {
        maybeMigrateLegacyResourceSettings();
    }

    ui::resources::ResourceSettings out{};
    QSettings settings = settingsStore();

    const std::optional<QString> projectNameOpt = ui::resources::settingsProjectName(kind);
    if (!projectNameOpt)
    {
        return out;
    }
    const QString projectName = *projectNameOpt;
    const QString rootsKey = QString("resources/%1/roots").arg(projectName);
    const QString gobsKey = QString("resources/%1/gobs").arg(projectName);

    QStringList roots = settings.value(rootsKey).toStringList();
    QStringList gobs = settings.value(gobsKey).toStringList();

    if (allowLegacyFallback)
    {
        // Backwards compatibility: fall back to legacy keys for any unset category.
        if (roots.isEmpty())
        {
            roots = settings.value("resources/roots").toStringList();
        }
        if (gobs.isEmpty())
        {
            gobs = settings.value("resources/gobs").toStringList();
        }
    }

    out.roots.reserve(static_cast<std::size_t>(roots.size()));
    for (const QString& root : roots)
    {
        if (root.trimmed().isEmpty())
        {
            continue;
        }
        out.roots.push_back(std::filesystem::path(root.toStdWString()));
    }

    out.gobs.reserve(static_cast<std::size_t>(gobs.size()));
    for (const QString& gob : gobs)
    {
        if (gob.trimmed().isEmpty())
        {
            continue;
        }
        out.gobs.push_back(std::filesystem::path(gob.toStdWString()));
    }

    return out;
}

ui::resources::ResourceSettings readResourceSettings(const ProjectType kind)
{
    return readResourceSettings(kind, false);
}

void writeResourceSettings(const ProjectType kind, const ui::resources::ResourceSettings& settings)
{
    QSettings store = settingsStore();

    const std::optional<QString> projectNameOpt = ui::resources::settingsProjectName(kind);
    if (!projectNameOpt)
    {
        return;
    }
    const QString projectName = *projectNameOpt;
    const QString rootsKey = QString("resources/%1/roots").arg(projectName);
    const QString gobsKey = QString("resources/%1/gobs").arg(projectName);

    QStringList roots;
    roots.reserve(static_cast<int>(settings.roots.size()));
    for (const std::filesystem::path& root : settings.roots)
    {
        if (root.empty())
        {
            continue;
        }
        std::error_code ec;
        const std::filesystem::path abs = std::filesystem::absolute(root, ec).lexically_normal();
        roots.push_back(QString::fromStdWString((!ec ? abs : root).wstring()));
    }

    QStringList gobs;
    gobs.reserve(static_cast<int>(settings.gobs.size()));
    for (const std::filesystem::path& gob : settings.gobs)
    {
        if (gob.empty())
        {
            continue;
        }
        std::error_code ec;
        const std::filesystem::path abs = std::filesystem::absolute(gob, ec).lexically_normal();
        gobs.push_back(QString::fromStdWString((!ec ? abs : gob).wstring()));
    }

    store.setValue(rootsKey, roots);
    store.setValue(gobsKey, gobs);
}

void maybeAutoConfigureProjectResources(const ProjectType kind)
{
    switch (kind)
    {
    case ProjectType::JKDF2:
    case ProjectType::MOTS:
    case ProjectType::IJIM:
        break;
    default:
        return;
    }

    const ui::resources::ResourceSettings existing = readResourceSettings(kind, false);
    if (!existing.roots.empty() || !existing.gobs.empty())
    {
        return;
    }

    const auto installDir = tryGetProjectInstallDir(kind);
    if (!installDir)
    {
        return;
    }

    if (!readProjectInstallDirSetting(kind))
    {
        writeProjectInstallDirSetting(kind, installDir);
    }

    const ui::resources::ResourceSettings detected = ui::resources::findDefaultProjectResources(kind, *installDir);
    if (detected.roots.empty() && detected.gobs.empty())
    {
        return;
    }

    writeResourceSettings(kind, detected);
}

ProjectType determineInitialProjectKind()
{
    const auto installDirExists = [](const std::optional<std::filesystem::path>& dir) noexcept -> bool
    {
        if (!dir || dir->empty())
        {
            return false;
        }

        std::error_code ec;
        return std::filesystem::exists(*dir, ec) && !ec;
    };

    const std::array<ProjectType, 3> orderedKinds{
        ProjectType::JKDF2,
        ProjectType::MOTS,
        ProjectType::IJIM,
    };

    std::vector<ProjectType> configured{};
    for (const ProjectType kind : orderedKinds)
    {
        const std::optional<std::filesystem::path> installDir =
            readProjectInstallDirSetting(kind);
        if (installDirExists(installDir)
            && ui::resources::validateProjectInstallPath(kind, *installDir)
                .canUse())
        {
            configured.push_back(kind);
        }
    }

    if (configured.empty())
    {
        std::vector<ProjectType> detected{};
        for (const ProjectType kind : orderedKinds)
        {
            if (installDirExists(tryGetProjectInstallDir(kind)))
            {
                detected.push_back(kind);
            }
        }

        if (!detected.empty())
        {
            if (detected.size() == 1)
            {
                return detected.front();
            }

            if (const auto last = readLastProjectKindSetting())
            {
                if (std::find(detected.begin(), detected.end(), *last) != detected.end())
                {
                    return *last;
                }
            }

            return detected.front();
        }

        if (const auto last = readLastProjectKindSetting(); last && ui::resources::isValidProjectType(*last))
        {
            return *last;
        }
        return ProjectType::JKDF2;
    }

    if (configured.size() == 1)
    {
        return configured.front();
    }

    if (const auto last = readLastProjectKindSetting())
    {
        if (std::find(configured.begin(), configured.end(), *last) != configured.end())
        {
            return *last;
        }
    }

    return configured.front();
}

} // namespace
