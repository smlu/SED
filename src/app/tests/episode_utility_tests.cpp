#include "test_common.hpp"

#include "ui/episode/episode_editor.hpp"

#include <filesystem>
#include <limits>
#include <stdexcept>

#include <QFile>
#include <QString>
#include <QTemporaryDir>

namespace sed::app_tests
{

void runEpisodeUtilityTests()
{
    check(ui::episode::startupCogGenerationSupported(ProjectType::JKDF2)
            && !ui::episode::startupCogGenerationSupported(ProjectType::MOTS)
            && !ui::episode::startupCogGenerationSupported(ProjectType::IJIM),
        "episode startup: JK inventory generator is gated away from MotS and IJIM");
    check(ui::episode::goalBaseForLevelNumber(21) == 21000
            && !ui::episode::goalBaseForLevelNumber(0)
            && !ui::episode::goalBaseForLevelNumber(-1)
            && !ui::episode::goalBaseForLevelNumber(std::numeric_limits<int>::max()),
        "episode goals: validates the legacy level-number inventory range before multiplication");

    {
        const std::string legacyByte(1, static_cast<char>(0xe9));
        const QString display = ui::episode::displayTextForGameBytes(legacyByte);
        const std::optional<std::string> roundTrip = ui::episode::gameBytesForDisplayText(display);
        const std::optional<std::string> unsupported = ui::episode::gameBytesForDisplayText(
            QString(QChar(0x20ac)));
        check(display == QString(QChar(0x00e9))
                && roundTrip && *roundTrip == legacyByte
                && !unsupported,
            "episode text: legacy one-byte characters round-trip and unrepresentable Unicode is rejected");
    }

    {
        assets::EpisodeSequence sequence{};
        sequence.levelNumber = 3;
        sequence.fileName = "valley.jkl";

        ui::episode::StartupCogOptions options{};
        options.forceRank = 4;
        options.goals = { "Find the key", "Escape" };
        options.bryar = true;
        options.thermalDetonators = true;
        options.sequencer = true;

        const std::string cog = ui::episode::generateStartupCogText(sequence, options);
        check(ui::episode::startupCogFileName(sequence) == "valley_start.cog",
            "episode startup: derives the legacy startup COG file name");
        check(cog.find("SetInv(player, 99, 3000);") != std::string::npos
                && cog.find("SetGoalFlags(player, 0, 1);") != std::string::npos
                && cog.find("SetGoalFlags(player, 1, 1);") != std::string::npos,
            "episode startup: emits level-scoped goal initialization");
        check(cog.find("SetInv(player, 2, 1);") != std::string::npos
                && cog.find("SetInv(player, 4, 1);") != std::string::npos
                && cog.find("SetInv(player, 8, 10);") != std::string::npos
                && cog.find("SetInv(player, 20, 4);") != std::string::npos,
            "episode startup: mirrors legacy selected-equipment quantities and Force rank");
        const std::size_t timerArm = cog.find("SetTimer(15);");
        const std::size_t timerLabel = cog.find("timer:", timerArm);
        check(timerArm != std::string::npos
                && timerLabel != std::string::npos
                && cog.substr(timerArm, timerLabel - timerArm).find("Return;") == std::string::npos,
            "episode startup: falls through once into Force setup before the 15-second repeat");
    }

    {
        assets::CogStringsFile strings{};
        strings.set("GOAL_03000", "Old A");
        strings.set("GOAL_03001", "Old B");
        strings.set("GOAL_04000", "Other level");
        strings.set("LEVEL03", "Level title");

        ui::episode::replaceGoalStrings(strings, 3000, { "New A" });
        check(strings.find("GOAL_03000") && *strings.find("GOAL_03000") == "New A"
                && strings.find("GOAL_03001") == nullptr,
            "episode goals: replaces the selected level's prior goal range");
        check(strings.find("GOAL_04000") && *strings.find("GOAL_04000") == "Other level"
                && strings.find("LEVEL03"),
            "episode goals: preserves other levels and unrelated strings");

        strings.set("GOAL_100000", "Old high A");
        strings.set("GOAL_100001", "Old high B");
        strings.set("GOAL_100050", "Out-of-objective-range metadata");
        ui::episode::replaceGoalStrings(strings, 100000, { "High A", {}, "High C" });
        check(strings.find("GOAL_100000") && *strings.find("GOAL_100000") == "High A"
                && !strings.find("GOAL_100001")
                && strings.find("GOAL_100002") && *strings.find("GOAL_100002") == "High C"
                && strings.find("GOAL_100050"),
            "episode goals: supports six-digit GOAL keys, sparse slots, and preserves keys beyond 50 objectives");

        bool rejectedTooManyGoals = false;
        try
        {
            ui::episode::StartupCogOptions tooMany{};
            tooMany.goals.resize(51, "Goal");
            assets::EpisodeSequence level{};
            level.fileName = "level.jkl";
            (void)ui::episode::generateStartupCogText(level, tooMany);
        }
        catch (const std::invalid_argument&)
        {
            rejectedTooManyGoals = true;
        }
        check(rejectedTooManyGoals,
            "episode startup: rejects more than the runtime's 50 objective slots");
    }

    {
        QTemporaryDir temporary{};
        check(temporary.isValid(), "episode project path: creates test directory");
        if (temporary.isValid())
        {
            const std::filesystem::path root(temporary.path().toStdWString());
            const std::filesystem::path jkl = root / "Jkl";
            std::error_code ec{};
            std::filesystem::create_directories(jkl, ec);

            const std::filesystem::path episodePath = root / "episode.jk";
            std::string error{};
            check(ui::episode::writeTextFileAtomically(episodePath, "episode", &error),
                "episode project path: atomic byte-preserving write succeeds");
            check(ui::episode::projectDirectoryForLevel(jkl / "level.jkl") == root,
                "episode project path: resolves the parent of a Jkl directory");
            const std::filesystem::path nestedJkl = root / "episodes" / "chapter1" / "Jkl";
            std::filesystem::create_directories(nestedJkl, ec);
            check(ui::episode::projectDirectoryForLevel(nestedJkl / "deep.jkl") == root,
                "episode project path: walks to the nearest campaign root through deep layouts");
            check(ui::episode::existingCogStringsPath(root) == root / "misc" / "cogstrings.uni",
                "episode project path: defaults new strings to misc/cogstrings.uni");
            check(ui::episode::canonicalCogStringsPath(root) == root / "misc" / "cogstrings.uni",
                "episode project path: saves strings to the canonical misc directory");

            const std::filesystem::path rootStrings = root / "cogstrings.uni";
            check(ui::episode::writeTextFileAtomically(rootStrings, "legacy", &error)
                    && ui::episode::existingCogStringsPath(root) == rootStrings
                    && ui::episode::canonicalCogStringsPath(root) != rootStrings,
                "episode project path: root-level cogstrings remains load-only migration fallback");

            std::filesystem::remove(rootStrings, ec);
            const std::filesystem::path uiStrings = root / "ui" / "cogstrings.uni";
            check(ui::episode::writeTextFileAtomically(uiStrings, "legacy ui", &error)
                    && ui::episode::existingCogStringsPath(root) == uiStrings,
                "episode project path: legacy ui/cogstrings is the final load-only fallback");

            QFile file(QString::fromStdWString(episodePath.wstring()));
            check(file.open(QIODevice::ReadOnly) && file.readAll() == QByteArray("episode"),
                "episode project path: atomic file contains complete payload");
        }
    }
}

} // namespace sed::app_tests
