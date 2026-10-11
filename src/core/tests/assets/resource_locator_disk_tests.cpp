#include "test_common.hpp"

#include "sed/assets/episode.hpp"
#include "sed/assets/jlh.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sed::tests
{

void runResourceLocatorDiskTests()
{
    const auto unique                      = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path root       = "tmp_res_root_" + unique;
    const std::filesystem::path file       = root / "mat" / "test.mat";
    const std::filesystem::path doubleFile = root / "mat" / "double.mat.mat";

    {
        sed::assets::ResourceLocator locator{};
        locator.addRoot({});
        check(locator.roots().empty(),
            "ResourceLocator.disk.roots ignores an empty root");

        locator.addRoot("first");
        locator.addRoot("second");
        check(locator.roots()
                == std::vector<std::filesystem::path>{ "first", "second" },
            "ResourceLocator.disk.roots preserves configured source order");
        locator.clearRoots();
        check(locator.roots().empty(),
            "ResourceLocator.disk.roots clear removes every configured root");
    }

    {
        sed::assets::ResourceLocator original;
        const auto emptyIdentity = original.previewCacheIdentity();
        original.addRoot({});
        original.clearRoots();
        original.clearGobs();
        original.reloadGobs();
        check(original.previewCacheIdentity() == emptyIdentity,
            "ResourceLocator cache identity is stable for configuration no-ops");
        original.addRoot("first");
        const auto firstIdentity = original.previewCacheIdentity();
        sed::assets::ResourceLocator copy = original;
        check(firstIdentity != emptyIdentity && copy.previewCacheIdentity() == firstIdentity,
            "ResourceLocator immutable copies retain their source configuration identity");
        original.addRoot("second");
        check(original.previewCacheIdentity() != firstIdentity
                && copy.previewCacheIdentity() == firstIdentity
                && copy.roots() == std::vector<std::filesystem::path>{"first"},
            "ResourceLocator successful mutation updates identity without changing retained copies");
        const auto beforeFailure = original.previewCacheIdentity();
        bool failed = false;
        try { original.mountGob(root / "absent-cache-identity.gob"); }
        catch ( const std::exception& ) { failed = true; }
        check(failed && original.previewCacheIdentity() == beforeFailure
                && original.gobs().empty(),
            "ResourceLocator failed archive mount preserves configuration identity");
        copy = original;
        check(copy.previewCacheIdentity() == original.previewCacheIdentity()
                && copy.roots() == original.roots(),
            "ResourceLocator assignment publishes configuration and hint identity together");
        auto movedFrom = copy;
        const auto transferredIdentity = movedFrom.previewCacheIdentity();
        auto movedTo = std::move(movedFrom);
        check(movedTo.previewCacheIdentity() == transferredIdentity
                && movedTo.roots() == copy.roots() && movedFrom.roots().empty()
                && movedFrom.gobs().empty() && movedFrom.previewCacheIdentity() == emptyIdentity,
            "ResourceLocator move transfers the hint identity and resets the empty source identity");
        movedFrom = std::move(movedTo);
        check(movedFrom.previewCacheIdentity() == transferredIdentity
                && movedFrom.roots() == copy.roots() && movedTo.roots().empty()
                && movedTo.gobs().empty() && movedTo.previewCacheIdentity() == emptyIdentity,
            "ResourceLocator move assignment preserves source and destination identity coherence");
        original.clearRoots();
        check(original.previewCacheIdentity() != beforeFailure && original.roots().empty()
                && copy.previewCacheIdentity() == beforeFailure && copy.roots().size() == 2,
            "ResourceLocator clearing invalidates only that mutable source's hints");
    }

    {
        sed::assets::ResourceLocator locator{};
        struct CandidateCase
        {
            std::string_view name;
            std::vector<std::string> expected;
        };
        const CandidateCase candidateCases[]{
            {
                "  folder\\effect.WAV  ",
                {
                    "folder/effect.WAV",
                    "sound/effect.WAV",
                    "voice/effect.WAV",
                    "voiceuu/effect.WAV",
                },
            },
            {
                "material.mat",
                {
                    "material.mat",
                    "mat/material.mat",
                    "3do/mat/material.mat",
                },
            },
            {
                "object.3DO",
                {
                    "object.3DO",
                    "3do/object.3DO",
                    "hi3do/object.3DO",
                },
            },
            { "items.dat", { "items.dat", "misc/items.dat" } },
            { "walk.key", { "walk.key", "3do/key/walk.key" } },
            { "actor.ai", { "actor.ai", "misc/ai/actor.ai" } },
            { "actor.ai0", { "actor.ai0", "misc/ai/actor.ai0" } },
            { "actor.ai2", { "actor.ai2", "misc/ai/actor.ai2" } },
            { "level.cmp", { "level.cmp", "misc/cmp/level.cmp" } },
            { "dust.par", { "dust.par", "misc/par/dust.par" } },
            { "rules.per", { "rules.per", "misc/per/rules.per" } },
            { "actor.pup", { "actor.pup", "misc/pup/actor.pup" } },
            { "actor.snd", { "actor.snd", "misc/snd/actor.snd" } },
            { "actor.spr", { "actor.spr", "misc/spr/actor.spr" } },
            { "startup.cog", { "startup.cog", "cog/startup.cog" } },
            { "master.tpl", { "master.tpl", "misc/master.tpl" } },
            {
                "cogstrings.uni",
                {
                    "cogstrings.uni",
                    "misc/cogstrings.uni",
                    "ui/cogstrings.uni",
                },
            },
            {
                "mat/already.mat",
                { "mat/already.mat", "3do/mat/already.mat" },
            },
            {
                "3do/mat/already.mat",
                { "3do/mat/already.mat", "mat/already.mat" },
            },
            {
                "sound/already.wav",
                {
                    "sound/already.wav",
                    "voice/already.wav",
                    "voiceuu/already.wav",
                },
            },
            {
                "3do/already.3do",
                { "3do/already.3do", "hi3do/already.3do" },
            },
            { "misc/cmp/already.cmp", { "misc/cmp/already.cmp" } },
            { "folder/unknown.bin", { "folder/unknown.bin" } },
        };
        bool allCandidatesMatch = true;
        for ( const CandidateCase& candidateCase : candidateCases )
        {
            allCandidatesMatch = allCandidatesMatch
                && locator.candidates(candidateCase.name)
                    == candidateCase.expected;
        }
        check(allCandidatesMatch,
            "ResourceLocator.disk.candidates preserves every kind folder, order, "
            "case, and rooted-path duplicate suppression");
        check(locator.candidates(" \t\r\n ").empty(),
            "ResourceLocator.disk.candidates rejects blank names");
    }

    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    check(!ec, "ResourceLocator.disk.create_directories");

    {
        std::ofstream out(file, std::ios::binary);
        if (!out)
        {
            check(false, "ResourceLocator.disk.create_file");
        }
        else
        {
            out.write("DISK", 4);
        }
    }

    {
        std::ofstream out(doubleFile, std::ios::binary);
        if (!out)
        {
            check(false, "ResourceLocator.disk.create_double_ext_file");
        }
        else
        {
            out.write("DOUBLE", 6);
        }
    }

    try
    {
        sed::assets::ResourceLocator locator{};
        locator.addRoot(root);

        auto bytesToString = [](const std::vector<std::byte>& bytes)
        {
            std::string out{};
            out.reserve(bytes.size());
            for (const std::byte b : bytes)
            {
                out.push_back(static_cast<char>(std::to_integer<unsigned char>(b)));
            }
            return out;
        };

        const auto bytes = locator.readBinary("test.mat");
        const std::string got = bytesToString(bytes);
        check(got == "DISK", "ResourceLocator.disk.readBinary");
        check(locator.readText("test.mat") == "DISK", "ResourceLocator.disk.readText");
        check(locator.readText("  mat\\test.mat  ") == "DISK",
            "ResourceLocator.disk.readText normalizes a trimmed backslash path");
        const auto found = locator.findOnDisk(" test.mat ");
        check(found.has_value() && *found == file,
            "ResourceLocator.disk.findOnDisk returns the exact path selected by candidate resolution");

        {
            const auto bytesOpt = locator.tryReadBinaryWithExt("test", ".mat");
            check(bytesOpt.has_value(), "ResourceLocator.disk.tryReadBinaryWithExt.append.has_value");
            check(bytesOpt ? (bytesToString(*bytesOpt) == "DISK") : false, "ResourceLocator.disk.tryReadBinaryWithExt.append.content");
        }
        check(locator.tryReadTextWithExt("test", ".mat").value_or("") == "DISK", "ResourceLocator.disk.tryReadTextWithExt.append");

        {
            const auto bytesOpt = locator.tryReadBinaryWithExt("test.mat", ".mat");
            check(bytesOpt.has_value(), "ResourceLocator.disk.tryReadBinaryWithExt.with_ext.has_value");
            check(bytesOpt ? (bytesToString(*bytesOpt) == "DISK") : false, "ResourceLocator.disk.tryReadBinaryWithExt.with_ext.content");
        }
        check(locator.tryReadTextWithExt("test.mat", ".mat").value_or("") == "DISK", "ResourceLocator.disk.tryReadTextWithExt.with_ext");

        check(!locator.tryReadBinaryWithExt("double.mat", ".mat").has_value(), "ResourceLocator.disk.tryReadBinaryWithExt.no_double_ext");
        check(!locator.tryReadTextWithExt("double.mat", ".mat").has_value(), "ResourceLocator.disk.tryReadTextWithExt.no_double_ext");
        check(!locator.tryReadBinary("missing.mat").has_value()
                && !locator.tryReadText("missing.mat").has_value()
                && !locator.findOnDisk("missing.mat").has_value(),
            "ResourceLocator.disk unavailable try/find operations remain non-throwing");
        checkThrowsWithMessage(
            [&locator]
            {
                (void)locator.readBinary("missing.mat");
            },
            "Resource not found: missing.mat",
            "ResourceLocator.disk readBinary reports the requested unavailable resource");
        checkThrowsWithMessage(
            [&locator]
            {
                (void)locator.readText("missing.mat");
            },
            "Resource not found: missing.mat",
            "ResourceLocator.disk readText reports the requested unavailable resource");

        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out.write("CHANGED", 7);
        }
        check(bytesToString(bytes) == "DISK"
                && locator.readText("test.mat") == "CHANGED",
            "ResourceLocator.disk rereads a same-path external replacement while a previously returned byte snapshot remains stable");

        std::filesystem::remove(file, ec);
        check(!ec
                && !locator.tryReadText("test.mat").has_value()
                && bytesToString(bytes) == "DISK",
            "ResourceLocator.disk observes external removal without invalidating an existing caller-owned byte snapshot");
    }
    catch (const std::exception& e)
    {
        check(false, e.what());
    }

    std::filesystem::remove_all(root, ec);

    {
        const std::filesystem::path root1 = "tmp_res_root1_" + unique;
        const std::filesystem::path root2 = "tmp_res_root2_" + unique;

        const std::filesystem::path file1 = root1 / "mat" / "test.mat";
        const std::filesystem::path file2 = root2 / "mat" / "test.mat";

        std::filesystem::create_directories(file1.parent_path(), ec);
        check(!ec, "ResourceLocator.disk.precedence.create_directories1");
        std::filesystem::create_directories(file2.parent_path(), ec);
        check(!ec, "ResourceLocator.disk.precedence.create_directories2");

        {
            std::ofstream out(file1, std::ios::binary);
            out.write("FIRST", 5);
        }
        {
            std::ofstream out(file2, std::ios::binary);
            out.write("SECOND", 6);
        }

        try
        {
            sed::assets::ResourceLocator locator{};
            locator.addRoot(root1);
            locator.addRoot(root2);

            check(locator.readText("test.mat") == "SECOND", "ResourceLocator.disk.precedence.last_root_wins");
            check(locator.tryReadTextWithExt("test", ".mat").value_or("")
                    == "SECOND",
                "ResourceLocator.disk.precedence.tryReadTextWithExt");
        }
        catch ( const std::exception& e )
        {
            check(false, e.what());
        }

        std::filesystem::remove_all(root1, ec);
        std::filesystem::remove_all(root2, ec);
    }

    {
        const std::filesystem::path firstRoot =
            "tmp_res_snapshot_first_" + unique;
        const std::filesystem::path secondRoot =
            "tmp_res_snapshot_second_" + unique;
        const std::filesystem::path firstFile =
            firstRoot / "mat" / "snapshot.mat";
        const std::filesystem::path secondFile =
            secondRoot / "mat" / "snapshot.mat";
        std::filesystem::create_directories(firstFile.parent_path(), ec);
        check(!ec,
            "ResourceLocator.disk.snapshot creates first root");
        std::filesystem::create_directories(secondFile.parent_path(), ec);
        check(!ec,
            "ResourceLocator.disk.snapshot creates second root");
        {
            std::ofstream out(firstFile, std::ios::binary);
            out.write("FIRST", 5);
        }
        {
            std::ofstream out(secondFile, std::ios::binary);
            out.write("SECOND", 6);
        }

        try
        {
            std::shared_ptr<const sed::assets::ResourceLocator>
                retainedSnapshot{};
            {
                sed::assets::ResourceLocator publisher{};
                publisher.addRoot(firstRoot);
                retainedSnapshot = std::make_shared<
                    const sed::assets::ResourceLocator>(publisher);

                publisher.clearRoots();
                publisher.addRoot(secondRoot);
                check(retainedSnapshot->roots()
                            == std::vector<std::filesystem::path>{ firstRoot }
                        && publisher.roots()
                            == std::vector<std::filesystem::path>{ secondRoot }
                        && retainedSnapshot->readText("snapshot.mat")
                            == "FIRST"
                        && publisher.readText("snapshot.mat") == "SECOND",
                    "ResourceLocator.disk.snapshot keeps an immutable root configuration while a new publisher configuration is installed");

                {
                    std::ofstream out(
                        firstFile,
                        std::ios::binary | std::ios::trunc);
                    out.write("FIRST-CHANGED", 13);
                }
                check(retainedSnapshot->readText("snapshot.mat")
                            == "FIRST-CHANGED"
                        && publisher.readText("snapshot.mat") == "SECOND",
                    "ResourceLocator.disk.snapshot retains source ownership but still observes an external file replacement in its own root");

                publisher.clearRoots();
                check(publisher.roots().empty()
                        && retainedSnapshot->roots()
                            == std::vector<std::filesystem::path>{ firstRoot }
                        && retainedSnapshot->readText("snapshot.mat")
                            == "FIRST-CHANGED",
                    "ResourceLocator.disk.snapshot outlives later publisher clearing without sharing mutable root vectors");
            }
            check(retainedSnapshot
                    && retainedSnapshot->roots()
                        == std::vector<std::filesystem::path>{ firstRoot }
                    && retainedSnapshot->readText("snapshot.mat")
                        == "FIRST-CHANGED",
                "ResourceLocator.disk.snapshot remains usable after the publishing locator is destroyed");
        }
        catch (const std::exception& e)
        {
            check(false, e.what());
        }

        std::filesystem::remove_all(firstRoot, ec);
        std::filesystem::remove_all(secondRoot, ec);
    }

    {
        const std::filesystem::path root1 = "tmp_res_exact1_" + unique;
        const std::filesystem::path root2 = "tmp_res_exact2_" + unique;
        const std::filesystem::path exact = root1 / "priority.mat";
        const std::filesystem::path laterFallback = root2 / "mat" / "priority.mat";

        std::filesystem::create_directories(exact.parent_path(), ec);
        check(!ec, "ResourceLocator.disk.candidate_precedence.create_directories1");
        std::filesystem::create_directories(laterFallback.parent_path(), ec);
        check(!ec, "ResourceLocator.disk.candidate_precedence.create_directories2");
        {
            std::ofstream out(exact, std::ios::binary);
            out.write("EXACT", 5);
        }
        {
            std::ofstream out(laterFallback, std::ios::binary);
            out.write("FALLBACK", 8);
        }

        try
        {
            sed::assets::ResourceLocator locator{};
            locator.addRoot(root1);
            locator.addRoot(root2);

            check(locator.readText("priority.mat") == "EXACT",
                "ResourceLocator.disk exact relative name outranks a later root's kind-folder fallback");
            const auto found = locator.findOnDisk("priority.mat");
            check(found.has_value() && *found == exact,
                "ResourceLocator.disk findOnDisk uses the same candidate-before-root precedence as reads");

            std::filesystem::remove(exact, ec);
            check(!ec && locator.readText("priority.mat") == "FALLBACK",
                "ResourceLocator.disk removal reveals the next deterministic candidate without rebuilding the locator");

            std::filesystem::create_directory(exact, ec);
            const auto foundAfterDirectoryReplacement =
                locator.findOnDisk("priority.mat");
            check(!ec
                    && locator.readText("priority.mat") == "FALLBACK"
                    && foundAfterDirectoryReplacement
                    && *foundAfterDirectoryReplacement == laterFallback,
                "ResourceLocator.disk a directory replacing a higher-priority file cannot shadow the next readable candidate");
        }
        catch ( const std::exception& e )
        {
            check(false, e.what());
        }

        std::filesystem::remove_all(root1, ec);
        std::filesystem::remove_all(root2, ec);
    }

    {
        const std::filesystem::path root1 = "tmp_res_graph_first_" + unique;
        const std::filesystem::path root2 = "tmp_res_graph_second_" + unique;
        auto writeTextFile = [](const std::filesystem::path& path,
                                 const std::string_view text)
        {
            std::error_code writeEc;
            std::filesystem::create_directories(path.parent_path(), writeEc);
            if ( writeEc )
            {
                return false;
            }

            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if ( !out )
            {
                return false;
            }
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            return static_cast<bool>(out);
        };

        const bool wroteGraph =
            writeTextFile(
                root1 / "master.tpl",
                "# DESC: Exact candidate template\n"
                "exact_actor none thingflags=0x10\n")
            && writeTextFile(
                root2 / "misc" / "master.tpl",
                "# DESC: Later-root fallback template\n"
                "fallback_actor none thingflags=0x20\n")
            && writeTextFile(
                root1 / "project_resources.jlh",
                "SUB First Resources\n"
                "LIST Actors First Actors\n"
                "SUBEND\n")
            && writeTextFile(
                root1 / "project_resources.jll",
                "[Actors]\n"
                "first_actor.3do\n")
            && writeTextFile(
                root2 / "project_resources.jlh",
                "SUB Selected Resources\n"
                "LIST Actors Selected Actors\n"
                "SUBEND\n")
            && writeTextFile(
                root2 / "project_resources.jll",
                "[Actors]\n"
                "selected_actor.3do\n")
            && writeTextFile(
                root1 / "episode.jk",
                "\"First Episode\"\n"
                "TYPE 1\n"
                "SEQ 1\n"
                "10: 0 1 LEVEL first.jkl 0 0 -1 -1\n"
                "END\n")
            && writeTextFile(
                root2 / "episode.jk",
                "\"Selected Episode\"\n"
                "TYPE 1\n"
                "SEQ 1\n"
                "10: 0 1 LEVEL selected.jkl 0 0 -1 -1\n"
                "END\n");
        check(wroteGraph,
            "ResourceLocator.disk.resource_graph writes deterministic loose fixtures");

        if ( wroteGraph )
        {
            try
            {
                sed::assets::ResourceLocator locator{};
                locator.addRoot(root1);
                locator.addRoot(root2);

                const sed::assets::Templates templates =
                    sed::assets::loadTemplates(locator, "master.tpl");
                const sed::assets::ThingTemplate* exactActor =
                    templates.find("EXACT_ACTOR");
                const sed::assets::JlhTree categories =
                    sed::assets::loadFromJLH("project_resources.jlh", locator);
                std::istringstream episodeInput(locator.readText("episode.jk"));
                const sed::assets::EpisodeFile episode =
                    sed::assets::loadFromEpisodeJk(episodeInput);

                check(exactActor
                        && exactActor->desc == "Exact candidate template"
                        && !templates.find("FALLBACK_ACTOR")
                        && categories.roots.size() == 1u
                        && categories.roots[0].label == "Selected Resources"
                        && categories.roots[0].children.size() == 1u
                        && categories.roots[0].children[0].list
                        && categories.roots[0].children[0].list->entries
                            == std::vector<std::string>{ "selected_actor.3do" }
                        && episode.name == "Selected Episode"
                        && episode.sequences.size() == 1u
                        && episode.sequences[0].fileName == "selected.jkl",
                    "ResourceLocator.disk.resource_graph feeds candidate-first and "
                    "last-root loose bytes through TPL, JLH/JLL, and episode parsers");
            }
            catch ( const std::exception& e )
            {
                check(false, e.what());
            }
        }

        std::filesystem::remove_all(root1, ec);
        std::filesystem::remove_all(root2, ec);
    }
}

} // namespace sed::tests
