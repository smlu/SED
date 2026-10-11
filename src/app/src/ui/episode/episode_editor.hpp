#pragma once

#include "sed/assets/episode.hpp"
#include "sed/level.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class QString;
class QWidget;

namespace sed::ui::episode
{

struct StartupCogOptions
{
    std::size_t sequenceIndex{};
    int forceRank{};
    std::vector<std::string> goals{};
    bool bryar{};
    bool rifle{};
    bool thermalDetonators{};
    bool crossbow{};
    bool repeater{};
    bool railgun{};
    bool sequencer{};
    bool concussion{};
    bool lightsaber{};
};

struct EpisodeEditorResult
{
    assets::EpisodeFile episode{};
    assets::CogStringsFile strings{};
    std::optional<StartupCogOptions> startupCog{};
};

/** Opens the complete JK/MotS campaign editor. Cancel returns `nullopt`. */
std::optional<EpisodeEditorResult> editEpisodeCampaign(
    QWidget* parent,
    assets::EpisodeFile episode,
    assets::CogStringsFile strings,
    std::string currentLevelFileName,
    ProjectType projectType);

/** Legacy startup inventory generation is reliable only for JKDF2 bins. */
bool startupCogGenerationSupported(ProjectType projectType) noexcept;

/** Lossless one-byte game text display conversion used by JK/MotS UNI files. */
QString displayTextForGameBytes(std::string_view value);

/** Returns null when a Qt string cannot be represented by the game's byte-wise text loader. */
std::optional<std::string> gameBytesForDisplayText(const QString& value);

/** Checked legacy GOAL_n namespace base (`levelNumber * 1000`). */
std::optional<int> goalBaseForLevelNumber(int levelNumber) noexcept;

/** Resolves `episode.jk` beside a JKL directory without assuming one project layout. */
std::filesystem::path projectDirectoryForLevel(const std::filesystem::path& levelPath);

/** Uses legacy root, then canonical `misc`, then legacy `ui` for loading. */
std::filesystem::path existingCogStringsPath(const std::filesystem::path& projectDirectory);

/** Canonical save location used by JK/MotS and the legacy SED migration path. */
std::filesystem::path canonicalCogStringsPath(const std::filesystem::path& projectDirectory);

std::string startupCogFileName(const assets::EpisodeSequence& sequence);
std::string generateStartupCogText(
    const assets::EpisodeSequence& sequence,
    const StartupCogOptions& options);

/** Replaces the selected level's GOAL_n strings without disturbing other levels. */
void replaceGoalStrings(
    assets::CogStringsFile& strings,
    int goalBase,
    const std::vector<std::string>& goals);

/** Creates parent directories and atomically replaces one byte-preserving text payload. */
bool writeTextFileAtomically(
    const std::filesystem::path& path,
    const std::string& text,
    std::string* error = nullptr);

} // namespace sed::ui::episode
