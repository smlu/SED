#include "test_common.hpp"

#include "sed/assets/episode.hpp"
#include "sed/assets/gob.hpp"
#include "sed/assets/jlh.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

bool writeGobV2(
    const std::filesystem::path& path,
    const std::vector<std::pair<std::string, std::string>>& entries)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if ( !out )
    {
        return false;
    }

    out.write("GOB ", 4);
    writeU32LE(out, 0x14u);
    writeU32LE(out, 0x0Cu);
    writeU32LE(out, static_cast<std::uint32_t>(entries.size()));

    std::uint32_t dataOffset = 16u
        + static_cast<std::uint32_t>(entries.size()) * 136u;
    for ( const auto& [name, data] : entries )
    {
        if ( name.size() > 128u )
        {
            return false;
        }

        writeU32LE(out, dataOffset);
        writeU32LE(out, static_cast<std::uint32_t>(data.size()));

        std::array<char, 128> nameBytes{};
        std::copy(name.begin(), name.end(), nameBytes.begin());
        out.write(nameBytes.data(), static_cast<std::streamsize>(nameBytes.size()));
        dataOffset += static_cast<std::uint32_t>(data.size());
    }

    for ( const auto& [name, data] : entries )
    {
        (void)name;
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    return static_cast<bool>(out);
}

bool writeGobV1(
    const std::filesystem::path& path,
    const std::string& name,
    const std::string& data)
{
    if ( name.size() > 13u )
    {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if ( !out )
    {
        return false;
    }

    const std::uint32_t dataOffset = 8u;
    const std::uint32_t indexOffset = dataOffset
        + static_cast<std::uint32_t>(data.size());
    out.write("GOB\n", 4);
    writeU32LE(out, indexOffset);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));

    writeU32LE(out, 1u);
    writeU32LE(out, dataOffset);
    writeU32LE(out, static_cast<std::uint32_t>(data.size()));
    std::array<char, 13> nameBytes{};
    std::copy(name.begin(), name.end(), nameBytes.begin());
    out.write(nameBytes.data(), static_cast<std::streamsize>(nameBytes.size()));
    return static_cast<bool>(out);
}

std::string bytesToString(const std::vector<std::byte>& bytes)
{
    std::string out{};
    out.reserve(bytes.size());
    for ( const std::byte byte : bytes )
    {
        out.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
    }
    return out;
}

} // namespace

void runGobTests()
{
    const auto unique                 = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path path  = "tmp_gob_" + unique + ".gob";

    const auto writeGob = [&](const std::string& name, const std::string& data)
    {
        return writeGobV2(path, { { name, data } });
    };

    check(writeGob("mat\\TEST.MAT", "HELLO"), "GobArchive.create.file");

    try
    {
        const sed::assets::GobArchive gob = sed::assets::GobArchive::open(path);
        check(gob.entries().size() == 1, "GobArchive.entries.size");
        check(gob.contains("mat/test.mat"), "GobArchive.contains.normalized");
        check(gob.contains("MAT\\TEST.MAT"), "GobArchive.contains.original");

        const auto bytes = gob.read("mat/test.mat");
        check(bytesToString(bytes) == "HELLO", "GobArchive.read.data");

        sed::assets::ResourceLocator locator{};
        locator.mountGob(path);

        const auto bytes2 = locator.readBinary("test.mat");
        check(bytesToString(bytes2) == "HELLO", "ResourceLocator.gob.readBinary");
        check(locator.readText("test.mat") == "HELLO", "ResourceLocator.gob.readText");

        check(writeGob("mat\\REPLACED.MAT", "WORLD!"), "ResourceLocator.reloadGobs.replace.file");
        locator.reloadGobs();
        check(!locator.findInGobs("test.mat").has_value()
                && locator.readText("replaced.mat") == "WORLD!"
                && bytesToString(bytes) == "HELLO"
                && bytesToString(bytes2) == "HELLO",
            "ResourceLocator.reloadGobs republishes a replacement archive index while prior archive and locator byte snapshots remain stable");

        check(writeGob("mat\\TRUNCATED.MAT", "WORLD!"),
            "ResourceLocator.reloadGobs.truncatedPayload.file");
        std::error_code truncateEc;
        std::filesystem::resize_file(path, 16u + 136u + 1u, truncateEc);
        bool rejectedTruncatedPayload = false;
        try
        {
            locator.reloadGobs();
        }
        catch (...)
        {
            rejectedTruncatedPayload = true;
        }
        check(!truncateEc && rejectedTruncatedPayload
                && locator.findInGobs("replaced.mat").has_value(),
            "ResourceLocator.reloadGobs rejects a header-complete archive with a truncated payload");

        {
            std::ofstream invalid(path, std::ios::binary | std::ios::trunc);
            invalid.write("PARTIAL", 7);
        }
        bool rejectedPartial = false;
        try
        {
            locator.reloadGobs();
        }
        catch (...)
        {
            rejectedPartial = true;
        }
        check(rejectedPartial && locator.findInGobs("replaced.mat").has_value(),
            "ResourceLocator.reloadGobs keeps every old index when replacement parsing fails");

        std::error_code removeEc;
        std::filesystem::remove(path, removeEc);
        bool rejectedMissing = false;
        try
        {
            locator.reloadGobs();
        }
        catch (...)
        {
            rejectedMissing = true;
        }
        check(rejectedMissing && locator.findInGobs("replaced.mat").has_value(),
            "ResourceLocator.reloadGobs keeps old indexes while an atomic replacement path is absent");

        check(writeGob("mat\\RETURNED.MAT", "BACK"), "ResourceLocator.reloadGobs.reappear.file");
        locator.reloadGobs();
        check(!locator.findInGobs("replaced.mat").has_value()
                && locator.readText("returned.mat") == "BACK",
            "ResourceLocator.reloadGobs accepts the archive when its replacement reappears");
    }
    catch (const std::exception& e)
    {
        check(false, e.what());
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);

    {
        const std::filesystem::path v1Path =
            "tmp_gob_v1_" + unique + ".gob";
        check(writeGobV1(v1Path, "TEST.MAT", "V1"),
            "GobArchive.v1 fixture writes a synthetic archive");
        try
        {
            const sed::assets::GobArchive v1 =
                sed::assets::GobArchive::open(v1Path);
            check(v1.entries().size() == 1u
                    && v1.contains("/test.mat")
                    && bytesToString(v1.read("TEST.MAT")) == "V1",
                "GobArchive.v1 normalizes leading separators and reads its indexed payload");
        }
        catch ( const std::exception& e )
        {
            check(false, e.what());
        }
        std::filesystem::remove(v1Path, ec);
    }

    {
        const std::filesystem::path duplicatePath =
            "tmp_gob_duplicate_" + unique + ".gob";
        check(writeGobV2(
                duplicatePath,
                {
                    { "mat\\same.mat", "FIRST" },
                    { "MAT/same.MAT", "SECOND" },
                }),
            "GobArchive.duplicate fixture writes case-equivalent entries");
        try
        {
            const sed::assets::GobArchive duplicate =
                sed::assets::GobArchive::open(duplicatePath);
            check(duplicate.entries().size() == 2u
                    && bytesToString(duplicate.read("mat/same.mat")) == "FIRST",
                "GobArchive.duplicate retains source entries while first normalized name wins lookup");
        }
        catch ( const std::exception& e )
        {
            check(false, e.what());
        }
        std::filesystem::remove(duplicatePath, ec);
    }

    {
        const std::filesystem::path firstPath =
            "tmp_gob_first_" + unique + ".gob";
        const std::filesystem::path secondPath =
            "tmp_gob_second_" + unique + ".gob";
        const std::filesystem::path looseRoot =
            "tmp_gob_loose_" + unique;
        const std::filesystem::path looseFile =
            looseRoot / "mat" / "shared.mat";

        check(writeGobV2(firstPath, { { "mat/shared.mat", "FIRST" } })
                && writeGobV2(secondPath, { { "mat/shared.mat", "SECOND" } }),
            "ResourceLocator.gob.precedence writes two mounted archives");
        std::filesystem::create_directories(looseFile.parent_path(), ec);
        check(!ec, "ResourceLocator.gob.precedence creates loose root");
        {
            std::ofstream out(looseFile, std::ios::binary);
            out.write("LOOSE", 5);
        }

        try
        {
            sed::assets::ResourceLocator locator{};
            locator.mountGob(firstPath);
            locator.mountGob(firstPath);
            check(locator.gobs().size() == 1u,
                "ResourceLocator.gob duplicate equivalent mount is ignored");
            locator.mountGob(secondPath);
            check(locator.readText("shared.mat") == "SECOND",
                "ResourceLocator.gob last mounted archive wins within one candidate");

            locator.addRoot(looseRoot);
            check(locator.readText("shared.mat") == "LOOSE",
                "ResourceLocator.gob loose resource wins over an archive for the same candidate");
            std::filesystem::remove(looseFile, ec);
            check(!ec && locator.readText("shared.mat") == "SECOND",
                "ResourceLocator.gob loose removal reveals the highest-priority mounted archive");

            locator.clearGobs();
            check(locator.gobs().empty()
                    && !locator.tryReadText("shared.mat").has_value(),
                "ResourceLocator.gob clear removes every mounted source");
        }
        catch ( const std::exception& e )
        {
            check(false, e.what());
        }

        std::filesystem::remove(firstPath, ec);
        std::filesystem::remove(secondPath, ec);
        std::filesystem::remove_all(looseRoot, ec);
    }

    {
        const std::filesystem::path firstPath =
            "tmp_gob_snapshot_first_" + unique + ".gob";
        const std::filesystem::path secondPath =
            "tmp_gob_snapshot_second_" + unique + ".gob";
        check(writeGobV2(
                  firstPath,
                  { { "mat/snapshot.mat", "FIRST" } })
                && writeGobV2(
                    secondPath,
                    { { "mat/snapshot.mat", "SECOND" } }),
            "ResourceLocator.gob.snapshot writes independent archive sources");

        try
        {
            std::shared_ptr<const sed::assets::ResourceLocator>
                retainedSnapshot{};
            {
                sed::assets::ResourceLocator publisher{};
                publisher.mountGob(firstPath);
                retainedSnapshot = std::make_shared<
                    const sed::assets::ResourceLocator>(publisher);

                publisher.clearGobs();
                publisher.mountGob(secondPath);
                check(retainedSnapshot->gobs().size() == 1u
                        && retainedSnapshot->gobs()[0].path() == firstPath
                        && publisher.gobs().size() == 1u
                        && publisher.gobs()[0].path() == secondPath
                        && retainedSnapshot->readText("snapshot.mat")
                            == "FIRST"
                        && publisher.readText("snapshot.mat") == "SECOND",
                    "ResourceLocator.gob.snapshot keeps an immutable mount configuration while a new publisher configuration is installed");

                publisher.clearGobs();
                check(publisher.gobs().empty()
                        && retainedSnapshot->gobs().size() == 1u
                        && retainedSnapshot->readText("snapshot.mat")
                            == "FIRST",
                    "ResourceLocator.gob.snapshot outlives later publisher clearing without sharing mutable archive indexes");
            }
            check(retainedSnapshot
                    && retainedSnapshot->gobs().size() == 1u
                    && retainedSnapshot->gobs()[0].path() == firstPath
                    && retainedSnapshot->readText("snapshot.mat") == "FIRST",
                "ResourceLocator.gob.snapshot remains usable after the publishing locator is destroyed");
        }
        catch (const std::exception& e)
        {
            check(false, e.what());
        }

        std::filesystem::remove(firstPath, ec);
        std::filesystem::remove(secondPath, ec);
    }

    {
        const std::filesystem::path resourcePath =
            "tmp_gob_resource_graph_" + unique + ".gob";
        check(writeGobV2(
                resourcePath,
                {
                    {
                        "misc/master.tpl",
                        "# DESC: Archive template\n"
                        "archive_actor none thingflags=0x10\n",
                    },
                    {
                        "project_resources.jlh",
                        "SUB Resources\n"
                        "LIST Actors Archive Actors\n"
                        "SUBEND\n",
                    },
                    {
                        "project_resources.jll",
                        "[Actors]\n"
                        "archive_actor.3do\n",
                    },
                    {
                        "episode.jk",
                        "\"Archive Episode\"\n"
                        "TYPE 1\n"
                        "SEQ 1\n"
                        "10: 0 1 LEVEL archive.jkl 0 0 -1 -1\n"
                        "END\n",
                    },
                }),
            "ResourceLocator.gob.resource_graph writes synthetic template, category-pair, and episode inputs");

        try
        {
            sed::assets::ResourceLocator locator{};
            locator.mountGob(resourcePath);

            const sed::assets::Templates templates =
                sed::assets::loadTemplates(locator, "master.tpl");
            const sed::assets::ThingTemplate* archiveActor =
                templates.find("ARCHIVE_ACTOR");
            const sed::assets::JlhTree categories =
                sed::assets::loadFromJLH("project_resources.jlh", locator);
            std::istringstream episodeInput(locator.readText("episode.jk"));
            const sed::assets::EpisodeFile episode =
                sed::assets::loadFromEpisodeJk(episodeInput);

            check(archiveActor
                    && archiveActor->desc == "Archive template"
                    && categories.roots.size() == 1u
                    && categories.roots[0].children.size() == 1u
                    && categories.roots[0].children[0].list
                    && categories.roots[0].children[0].list->entries
                        == std::vector<std::string>{ "archive_actor.3do" }
                    && episode.name == "Archive Episode"
                    && episode.sequences.size() == 1u
                    && episode.sequences[0].fileName == "archive.jkl",
                "ResourceLocator.gob.resource_graph feeds complete archive bytes "
                "through template, JLH/JLL, and episode parsers");
        }
        catch ( const std::exception& e )
        {
            check(false, e.what());
        }

        std::filesystem::remove(resourcePath, ec);
    }

    {
        const std::filesystem::path candidatePath =
            "tmp_gob_candidate_precedence_" + unique + ".gob";
        const std::filesystem::path looseRoot =
            "tmp_gob_candidate_precedence_" + unique;
        const std::filesystem::path looseFallback =
            looseRoot / "mat" / "priority.mat";
        check(writeGobV2(
                candidatePath,
                { { "priority.mat", "ARCHIVE_EXACT" } }),
            "ResourceLocator.gob.candidate_precedence writes the exact archive candidate");
        std::filesystem::create_directories(looseFallback.parent_path(), ec);
        check(!ec,
            "ResourceLocator.gob.candidate_precedence creates the loose fallback directory");
        {
            std::ofstream out(looseFallback, std::ios::binary);
            out.write("DISK_FALLBACK", 13);
        }

        try
        {
            sed::assets::ResourceLocator locator{};
            locator.addRoot(looseRoot);
            locator.mountGob(candidatePath);
            check(locator.readText("priority.mat") == "ARCHIVE_EXACT",
                "ResourceLocator.gob exact archive candidate outranks a loose kind-folder fallback");
        }
        catch ( const std::exception& e )
        {
            check(false, e.what());
        }

        std::filesystem::remove(candidatePath, ec);
        std::filesystem::remove_all(looseRoot, ec);
    }

    {
        const std::filesystem::path firstPath =
            "tmp_gob_reload_first_" + unique + ".gob";
        const std::filesystem::path secondPath =
            "tmp_gob_reload_second_" + unique + ".gob";
        check(writeGobV2(firstPath, { { "mat/old_first.mat", "A" } })
                && writeGobV2(secondPath, { { "mat/old_second.mat", "B" } }),
            "ResourceLocator.reloadGobs.multi fixture writes initial archives");

        try
        {
            sed::assets::ResourceLocator locator{};
            locator.mountGob(firstPath);
            locator.mountGob(secondPath);

            check(writeGobV2(firstPath, { { "mat/new_first.mat", "AA" } }),
                "ResourceLocator.reloadGobs.multi replaces first archive");
            {
                std::ofstream invalid(secondPath, std::ios::binary | std::ios::trunc);
                invalid.write("PARTIAL", 7);
            }

            bool rejected = false;
            try
            {
                locator.reloadGobs();
            }
            catch ( ... )
            {
                rejected = true;
            }
            check(rejected
                    && locator.findInGobs("old_first.mat").has_value()
                    && locator.findInGobs("old_second.mat").has_value()
                    && !locator.findInGobs("new_first.mat").has_value(),
                "ResourceLocator.reloadGobs.multi rejects all candidate indexes when any replacement fails");

            check(writeGobV2(secondPath, { { "mat/new_second.mat", "BB" } }),
                "ResourceLocator.reloadGobs.multi repairs second archive");
            locator.reloadGobs();
            check(!locator.findInGobs("old_first.mat").has_value()
                    && !locator.findInGobs("old_second.mat").has_value()
                    && locator.readText("new_first.mat") == "AA"
                    && locator.readText("new_second.mat") == "BB",
                "ResourceLocator.reloadGobs.multi publishes every replacement together after complete validation");
        }
        catch ( const std::exception& e )
        {
            check(false, e.what());
        }

        std::filesystem::remove(firstPath, ec);
        std::filesystem::remove(secondPath, ec);
    }

    {
        const std::filesystem::path invalidPath =
            "tmp_gob_invalid_" + unique + ".gob";
        {
            std::ofstream out(invalidPath, std::ios::binary | std::ios::trunc);
            out.write("GOB ", 4);
            writeU32LE(out, 0u);
            writeU32LE(out, 0u);
            writeU32LE(out, 1u);
            writeU32LE(out, 4096u);
            writeU32LE(out, 1u);
            std::array<char, 128> nameBytes{};
            nameBytes[0] = 'x';
            out.write(nameBytes.data(), static_cast<std::streamsize>(nameBytes.size()));
        }
        checkThrowsWithMessage(
            [&invalidPath]
            {
                (void)sed::assets::GobArchive::open(invalidPath);
            },
            "GOB entry extends past end of file: x in " + invalidPath.string(),
            "GobArchive.bounds rejects an indexed payload outside the archive before publication");

        {
            std::ofstream out(invalidPath, std::ios::binary | std::ios::trunc);
            out.write("GOB ", 4);
            writeU32LE(out, 0u);
            writeU32LE(out, 0u);
            writeU32LE(out, 1u);
            writeU32LE(out, 0u);
        }
        checkThrowsWithMessage(
            [&invalidPath]
            {
                (void)sed::assets::GobArchive::open(invalidPath);
            },
            "Unexpected EOF: GOB v2 index extends past end of file: " + invalidPath.string(),
            "GobArchive.truncated rejects an incomplete index record before publication");

        {
            std::ofstream out(invalidPath, std::ios::binary | std::ios::trunc);
            out.write("GOB ", 4);
            writeU32LE(out, 0u);
            writeU32LE(out, 0u);
            writeU32LE(out, std::numeric_limits<std::uint32_t>::max());
        }
        checkThrowsWithMessage(
            [&invalidPath]
            {
                (void)sed::assets::GobArchive::open(invalidPath);
            },
            "Unexpected EOF: GOB v2 index extends past end of file: " + invalidPath.string(),
            "GobArchive.count_bounds rejects an impossible entry count before reserving attacker-controlled storage");
        std::filesystem::remove(invalidPath, ec);
    }
}

} // namespace sed::tests
