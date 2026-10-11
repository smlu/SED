#include "test_common.hpp"

#include "sed/assets/episode.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sed::tests
{

void runEpisodeTests()
{
    {
        std::istringstream input(R"EP(
# JK episode fixture
"The Dark Path"
TYPE 1
SEQ 3

10: 0 1 LEVEL level01.jkl 22 30 20 -1
20: 0 2 CUT intro.smk 0 0 30 30
30: 0 3 DECIDE branch 0 0 10 20
end
)EP");

        const assets::EpisodeFile episode = assets::loadFromEpisodeJk(input);
        check(episode.name == "The Dark Path" && episode.type == 1,
            "episode.jk: parses name and episode type");
        check(episode.sequences.size() == 3,
            "episode.jk: parses declared sequence rows");
        if (episode.sequences.size() == 3)
        {
            check(episode.sequences[0].lineNumber == 10
                    && episode.sequences[0].type == "LEVEL"
                    && episode.sequences[0].fileName == "level01.jkl"
                    && episode.sequences[0].lightPower == 22
                    && episode.sequences[0].darkPower == 30
                    && episode.sequences[0].gotoA == 20
                    && episode.sequences[0].gotoB == -1,
                "episode.jk: preserves complete LEVEL sequence fields");
            check(episode.sequences[1].type == "CUT"
                    && episode.sequences[2].type == "DECIDE",
                "episode.jk: accepts CUT and DECIDE rows");
        }

        const std::string saved = assets::saveToEpisodeJkText(episode);
        std::istringstream roundTrip(saved);
        const assets::EpisodeFile loaded = assets::loadFromEpisodeJk(roundTrip);
        check(loaded.name == episode.name
                && loaded.type == episode.type
                && loaded.sequences.size() == episode.sequences.size()
                && loaded.sequences[2].gotoA == 10
                && loaded.sequences[2].gotoB == 20,
            "episode.jk: canonical save round-trips sequence graph");
    }

    {
        // Representative MotS records use SAN cutscenes and frequently point
        // gotoB back to the current LEVEL/next scenario line.
        std::istringstream input(R"EP(
"Mysteries of the Sith"
TYPE 1
SEQ 3
10:	1	1	LEVEL	s1l1_RebelBase.jkl	0	0	11	10
11:	1	1	CUT	s1l1ecs.san	0	0	20	20
20:	1	2	LEVEL	s1l2_AstCrust.jkl	0	0	-1	-1
end
)EP");
        const assets::EpisodeFile episode = assets::loadFromEpisodeJk(input);
        check(episode.sequences.size() == 3
                && episode.sequences[0].gotoB == 10
                && episode.sequences[1].fileName == "s1l1ecs.san",
            "episode.jk: parses representative MotS self/back branches and SAN cutscenes");
    }

    {
        bool rejected = false;
        try
        {
            std::istringstream truncated("\"Broken\"\nTYPE 1\nSEQ 2\n10: 0 1 LEVEL one.jkl 0 0 -1 -1\n");
            (void)assets::loadFromEpisodeJk(truncated);
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        check(rejected, "episode.jk: rejects a truncated declared sequence list");
    }

    {
        const auto rejectsEpisode = [](const std::string& text)
        {
            try
            {
                std::istringstream input(text);
                (void)assets::loadFromEpisodeJk(input);
                return false;
            }
            catch (const std::runtime_error&)
            {
                return true;
            }
        };
        check(rejectsEpisode("\"No end\"\nTYPE 1\nSEQ 1\n10: 0 1 LEVEL one.jkl 0 0 -1 -1\n"),
            "episode.jk: requires END after the declared sequence list");
        check(rejectsEpisode("\"Extra field\"\nTYPE 1\nSEQ 1\n10: 0 1 LEVEL one.jkl 0 0 -1 -1 surprise\nEND\n"),
            "episode.jk: rejects trailing sequence fields that saving would discard");
        check(rejectsEpisode("\"Extra tail\"\nTYPE 1\nSEQ 1\n10: 0 1 LEVEL one.jkl 0 0 -1 -1\nEND\nUNKNOWN\n"),
            "episode.jk: rejects meaningful content after END");
        check(rejectsEpisode("\"Duplicate\"\nTYPE 1\nSEQ 2\n10: 0 1 LEVEL one.jkl 0 0 -1 -1\n10: 0 2 LEVEL two.jkl 0 0 -1 -1\nEND\n"),
            "episode.jk: rejects duplicate sequence line identifiers");
        check(rejectsEpisode("\"Dangling\"\nTYPE 1\nSEQ 1\n10: 0 1 LEVEL one.jkl 0 0 20 -1\nEND\n"),
            "episode.jk: rejects dangling goto targets");
        check(rejectsEpisode("\"No colon\"\nTYPE 1\nSEQ 1\n10 0 1 LEVEL one.jkl 0 0 -1 -1\nEND\n"),
            "episode.jk: requires the runtime line-number colon grammar");
        check(rejectsEpisode("\"Misplaced colon\"\nTYPE 1\nSEQ 1\n10 0: 1 LEVEL one.jkl 0 0 -1 -1\nEND\n"),
            "episode.jk: rejects a colon outside the line-number field");
        check(rejectsEpisode("\"Spaced colon\"\nTYPE 1\nSEQ 1\n10 : 0 1 LEVEL one.jkl 0 0 -1 -1\nEND\n"),
            "episode.jk: requires the colon immediately after the line ID");
        check(rejectsEpisode("\"Reserved\"\nTYPE 1\nSEQ 1\n-1: 0 1 LEVEL one.jkl 0 0 -1 -1\nEND\n"),
            "episode.jk: rejects negative line IDs because -1 is a terminal branch sentinel");
        check(rejectsEpisode(
                  "\"Long file\"\nTYPE 1\nSEQ 1\n10: 0 1 LEVEL "
                  + std::string(32, 'a') + " 0 0 -1 -1\nEND\n"),
            "episode.jk: rejects filenames beyond the runtime 31-byte field");
        check(rejectsEpisode("\"Unknown type\"\nTYPE 1\nSEQ 1\n10: 0 1 MOVIE one.smk 0 0 -1 -1\nEND\n"),
            "episode.jk: rejects types the runtime would silently reinterpret as DECIDE");

        assets::EpisodeFile bounded{};
        bounded.name = std::string(60, 'N');
        bounded.sequences.push_back(assets::EpisodeSequence{
            10, 0, 1, "LEVEL", std::string(31, 'f'), 0, 0, -1, -1 });
        bool acceptedBounds = true;
        try
        {
            (void)assets::saveToEpisodeJkText(bounded);
        }
        catch (const std::runtime_error&)
        {
            acceptedBounds = false;
        }
        check(acceptedBounds,
            "episode.jk: accepts the runtime 60-byte name and 31-byte filename boundaries");
        bounded.name.push_back('N');
        bool rejectedLongName = false;
        try
        {
            (void)assets::saveToEpisodeJkText(bounded);
        }
        catch (const std::runtime_error&)
        {
            rejectedLongName = true;
        }
        check(rejectedLongName, "episode.jk: rejects names that exceed the discovery buffer");
    }

    {
        std::istringstream input(R"UNI(
MSGS 4
# comment
"LEVEL01" 0 "Valley of Fire"
"LEVEL01_TEXT_00" 0 "Reach the tower # this is text, not a comment"
"LEVEL01_TEXT_01" 0 "Find the key"
"GOAL_01000" 0 "Escape"
END
)UNI");

        assets::CogStringsFile strings = assets::loadFromCogStringsUni(input);
        const std::string* title = strings.find("level01");
        const std::string* text = strings.find("LEVEL01_TEXT_00");
        check(title && *title == "Valley of Fire",
            "cogstrings.uni: lookup is case-insensitive");
        check(text && text->find("# this is text") != std::string::npos,
            "cogstrings.uni: hash inside quoted text is preserved");

        strings.set("level01", "The Valley");
        strings.set("new_key", "New text");
        check(strings.erase("goal_01000") && strings.find("GOAL_01000") == nullptr,
            "cogstrings.uni: editable set/erase uses case-insensitive keys");

        const std::string saved = assets::saveToCogStringsUniText(strings);
        std::istringstream roundTrip(saved);
        const assets::CogStringsFile loaded = assets::loadFromCogStringsUni(roundTrip);
        check(loaded.entries.size() == 4
                && loaded.find("LEVEL01")
                && *loaded.find("LEVEL01") == "The Valley"
                && loaded.find("NEW_KEY")
                && *loaded.find("NEW_KEY") == "New text",
            "cogstrings.uni: canonical save round-trips edited strings");

        strings.set("LITERAL_PATH", R"(misc\episode\text)");
        const std::string pathText = assets::saveToCogStringsUniText(strings);
        std::istringstream pathRoundTrip(pathText);
        const assets::CogStringsFile pathStrings = assets::loadFromCogStringsUni(pathRoundTrip);
        check(pathStrings.find("LITERAL_PATH")
                && *pathStrings.find("LITERAL_PATH") == R"(misc\episode\text)"
                && pathText.find(R"("LITERAL_PATH" 0 "misc\episode\text")") != std::string::npos,
            "cogstrings.uni: preserves game-literal backslashes without C-style escaping");

        bool rejectedQuote = false;
        try
        {
            strings.set("QUOTED_TEXT", R"(The "quoted" value)");
            (void)assets::saveToCogStringsUniText(strings);
        }
        catch (const std::runtime_error&)
        {
            rejectedQuote = true;
        }
        check(rejectedQuote,
            "cogstrings.uni: rejects embedded quotes unsupported by the JK/MotS parser");

        assets::CogStringsFile whitespace{};
        whitespace.set("WHITESPACE", "  A\tB  ");
        const std::string whitespaceText = assets::saveToCogStringsUniText(whitespace);
        std::istringstream whitespaceRoundTrip(whitespaceText);
        const assets::CogStringsFile whitespaceLoaded = assets::loadFromCogStringsUni(whitespaceRoundTrip);
        check(whitespaceLoaded.find("WHITESPACE")
                && *whitespaceLoaded.find("WHITESPACE") == "  A\tB  ",
            "cogstrings.uni: preserves leading/trailing spaces and literal tabs inside text");
    }

    {
        assets::CogStringsFile root{};
        root.set("ROOT_ONLY", "root");
        root.set("SHARED", "root value");
        assets::CogStringsFile misc{};
        misc.set("MISC_ONLY", "misc");
        misc.set("SHARED", "misc value");

        const std::vector<std::string> conflicts =
            assets::conflictingCogStringKeys(root, misc);
        const assets::CogStringsFile rootPreferred =
            assets::mergeCogStrings(root, misc);
        const assets::CogStringsFile miscPreferred =
            assets::mergeCogStrings(misc, root);
        check(conflicts == std::vector<std::string>{ "SHARED" }
                && rootPreferred.find("ROOT_ONLY")
                && rootPreferred.find("MISC_ONLY")
                && *rootPreferred.find("SHARED") == "root value"
                && *miscPreferred.find("SHARED") == "misc value",
            "cogstrings.uni: dual-file migration merges unique keys and makes conflict precedence explicit");

        assets::CogStringsFile zeroMetadata{};
        zeroMetadata.set("META", "same text", 0);
        assets::CogStringsFile nonzeroMetadata{};
        nonzeroMetadata.set("META", "same text", 42);
        const std::vector<std::string> metadataConflicts =
            assets::conflictingCogStringKeys(zeroMetadata, nonzeroMetadata);
        assets::CogStringsFile metadataPreferred =
            assets::mergeCogStrings(nonzeroMetadata, zeroMetadata);
        metadataPreferred.set("META", "edited text");
        check(metadataConflicts == std::vector<std::string>{ "META" }
                && metadataPreferred.entries.size() == 1
                && metadataPreferred.entries[0].unusedNumber == 42,
            "cogstrings.uni: metadata differences conflict and ordinary text edits preserve the chosen number");
    }

    {
        std::istringstream capacityHeader("MSGS 1000\n\"ONE\" 0 \"First\"\nEND\n");
        const assets::CogStringsFile strings = assets::loadFromCogStringsUni(capacityHeader);
        check(strings.entries.size() == 1 && strings.find("ONE"),
            "cogstrings.uni: accepts the overdeclared MSGS capacity used by retail MotS");
    }

    {
        std::istringstream nonzeroField("MSGS 1\n\"ONE\" 42 \"First\"\nEND\n");
        const assets::CogStringsFile strings = assets::loadFromCogStringsUni(nonzeroField);
        const std::string saved = assets::saveToCogStringsUniText(strings);
        check(strings.entries.size() == 1
                && strings.entries[0].unusedNumber == 42
                && saved.find("\"ONE\" 42 \"First\"") != std::string::npos,
            "cogstrings.uni: preserves the legacy numeric message field even when nonzero");
    }

    {
        const auto rejectsStrings = [](const std::string& text)
        {
            try
            {
                std::istringstream input(text);
                (void)assets::loadFromCogStringsUni(input);
                return false;
            }
            catch (const std::runtime_error&)
            {
                return true;
            }
        };
        check(rejectsStrings("MSGS 1\n\"ONE\" 0 \"First\"\n"),
            "cogstrings.uni: requires END after the declared messages");
        check(rejectsStrings("MSGS 2\n\"ONE\" 0 \"First\"\n\"one\" 0 \"Second\"\nEND\n"),
            "cogstrings.uni: rejects case-insensitive duplicate keys instead of collapsing data");
        check(rejectsStrings("MSGS 1\n\"ONE\" 0 \"First\" trailing\nEND\n"),
            "cogstrings.uni: rejects trailing message fields that saving would discard");
        check(rejectsStrings("MSGS 1 trailing\n\"ONE\" 0 \"First\"\nEND\n"),
            "cogstrings.uni: rejects trailing MSGS header fields");
        check(rejectsStrings("MSGS 1\n\"ONE\" unused \"First\"\nEND\n"),
            "cogstrings.uni: requires the legacy unused field to remain numeric");
        check(rejectsStrings("MSGS 1\n\"ONE\" 0 \"First\"\n\"TWO\" 0 \"Second\"\nEND\n"),
            "cogstrings.uni: rejects records beyond the runtime MSGS capacity");
        check(rejectsStrings(""),
            "cogstrings.uni: rejects a truncated empty existing file");
        check(rejectsStrings("MSGS 1\n\"\" 0 \"value\"\nEND\n"),
            "cogstrings.uni: rejects an empty quoted key instead of dropping it");
        std::string nulRecord = "MSGS 1\n\"KEY\" 0 \"A";
        nulRecord.push_back('\0');
        nulRecord += "B\"\nEND\n";
        check(rejectsStrings(nulRecord),
            "cogstrings.uni: rejects embedded NUL bytes that truncate runtime C strings");

        assets::CogStringsFile invalidModel{};
        invalidModel.entries.push_back({ "", "empty key" });
        bool rejectedEmptyModelKey = false;
        try
        {
            (void)assets::saveToCogStringsUniText(invalidModel);
        }
        catch (const std::runtime_error&)
        {
            rejectedEmptyModelKey = true;
        }
        invalidModel.entries = { { "DUP", "one" }, { "dup", "two" } };
        bool rejectedDuplicateModelKey = false;
        try
        {
            (void)assets::saveToCogStringsUniText(invalidModel);
        }
        catch (const std::runtime_error&)
        {
            rejectedDuplicateModelKey = true;
        }
        check(rejectedEmptyModelKey && rejectedDuplicateModelKey,
            "cogstrings.uni: public model serialization rejects empty and duplicate keys without dropping data");

        assets::CogStringsFile lineBoundary{};
        lineBoundary.set("K", std::string(245, 'x'));
        bool acceptedLineBoundary = true;
        try
        {
            (void)assets::saveToCogStringsUniText(lineBoundary);
        }
        catch (const std::runtime_error&)
        {
            acceptedLineBoundary = false;
        }
        lineBoundary.set("K", std::string(246, 'x'));
        bool rejectedLongLine = false;
        try
        {
            (void)assets::saveToCogStringsUniText(lineBoundary);
        }
        catch (const std::runtime_error&)
        {
            rejectedLongLine = true;
        }
        check(acceptedLineBoundary && rejectedLongLine,
            "cogstrings.uni: enforces the runtime 253-byte record boundary");
    }

    {
        const std::string unique = std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const std::filesystem::path episodePath = "tmp_episode_validation_" + unique + ".jk";
        const std::filesystem::path stringsPath = "tmp_cogstrings_validation_" + unique + ".uni";
        {
            std::ofstream episodeOut(episodePath, std::ios::binary);
            episodeOut << "episode sentinel";
            std::ofstream stringsOut(stringsPath, std::ios::binary);
            stringsOut << "strings sentinel";
        }

        assets::EpisodeFile invalidEpisode{};
        invalidEpisode.sequences.push_back(assets::EpisodeSequence{
            10, 0, 1, "LEVEL", std::string(32, 'f'), 0, 0, -1, -1 });
        assets::CogStringsFile invalidStrings{};
        invalidStrings.entries.push_back({ "KEY", "bad \" text" });
        try
        {
            assets::saveToEpisodeJk(invalidEpisode, episodePath);
        }
        catch (const std::runtime_error&)
        {
        }
        try
        {
            assets::saveToCogStringsUni(invalidStrings, stringsPath);
        }
        catch (const std::runtime_error&)
        {
        }

        {
            std::ifstream episodeIn(episodePath, std::ios::binary);
            std::ifstream stringsIn(stringsPath, std::ios::binary);
            const std::string episodeContents{
                std::istreambuf_iterator<char>(episodeIn), std::istreambuf_iterator<char>() };
            const std::string stringsContents{
                std::istreambuf_iterator<char>(stringsIn), std::istreambuf_iterator<char>() };
            check(episodeContents == "episode sentinel" && stringsContents == "strings sentinel",
                "episode serializers: validation failure leaves existing files untouched");
        }

        std::error_code ec{};
        std::filesystem::remove(episodePath, ec);
        ec.clear();
        std::filesystem::remove(stringsPath, ec);
    }
}

} // namespace sed::tests
