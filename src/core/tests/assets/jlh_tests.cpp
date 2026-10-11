#include "test_common.hpp"

#include "sed/assets/jlh.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sed::tests
{

void runJlhTests()
{
    {
        const char* jll = R"JLL(
        [Water]
        a.mat
        b.mat

        [Sky]
        x.mat
        )JLL";

        const char* jlh = R"JLH(
        SUB Textures
        LIST Water Water
        LIST Sky Sky
        SUBEND
        )JLH";

        std::istringstream jllStream(jll);
        const auto lists = sed::assets::parseJLL(jllStream);

        check(lists.size() == 2, "JLH: lists.size");
        check(lists.contains("WATER"), "JLH: lists[WATER] exists");
        check(lists.contains("SKY"), "JLH: lists[SKY] exists");

        if (lists.contains("WATER"))
        {
            const auto& list = lists.at("WATER");
            check(list != nullptr, "JLH: WATER list ptr");
            if (list)
            {
                check(list->name == "Water", "JLH: WATER list name");
                check(list->entries.size() == 2, "JLH: WATER entries.size");
                if (list->entries.size() == 2)
                {
                    check(list->entries[0] == "a.mat", "JLH: WATER entries[0]");
                    check(list->entries[1] == "b.mat", "JLH: WATER entries[1]");
                }
            }
        }

        std::istringstream jlhStream(jlh);
        const sed::assets::JlhTree tree = sed::assets::parseJLH(jlhStream, lists);

        check(tree.roots.size() == 1, "JLH: roots.size");
        if (tree.roots.size() == 1)
        {
            const sed::assets::JlhNode& root = tree.roots[0];
            check(root.label == "Textures", "JLH: root.label");
            check(root.children.size() == 2, "JLH: root.children.size");
            if (root.children.size() == 2)
            {
                const sed::assets::JlhNode& water = root.children[0];
                check(water.label == "Water", "JLH: root.children[0].label");
                check(water.list != nullptr, "JLH: root.children[0].list");

                const sed::assets::JlhNode& sky = root.children[1];
                check(sky.label == "Sky", "JLH: root.children[1].label");
                check(sky.list != nullptr, "JLH: root.children[1].list");
            }
        }
    }

    {
        const char* jll = R"JLL(
        [Foo]
        one
        )JLL";

        const char* jlh = R"JLH(
        sub Root
        list foo Visible
        list Missing Hidden
        subend
        )JLH";

        std::istringstream jllStream(jll);
        const auto lists = sed::assets::parseJLL(jllStream);

        std::istringstream jlhStream(jlh);
        const sed::assets::JlhTree tree = sed::assets::parseJLH(jlhStream, lists);

        check(tree.roots.size() == 1, "JLH.case: roots.size");
        if (tree.roots.size() == 1)
        {
            const sed::assets::JlhNode& root = tree.roots[0];
            check(root.label == "Root", "JLH.case: root.label");
            check(root.children.size() == 1, "JLH.case: root.children.size");
            if (root.children.size() == 1)
            {
                const sed::assets::JlhNode& visible = root.children[0];
                check(visible.label == "Visible", "JLH.case: child.label");
                check(visible.list != nullptr, "JLH.case: child.list");
            }
        }
    }

    {
        auto ambience = std::make_shared<sed::assets::JllList>();
        ambience->name = "ambience";
        ambience->entries = { "wind.wav", "water.wav" };

        sed::assets::JlhTree original{};
        sed::assets::JlhNode sounds{};
        sounds.label = "Sounds";
        sed::assets::JlhNode gameplay{};
        gameplay.label = "Gameplay";
        gameplay.children.push_back(sed::assets::JlhNode{ "Ambience", ambience, {} });
        sounds.children.push_back(std::move(gameplay));
        original.roots.push_back(std::move(sounds));

        std::ostringstream jlhOut;
        std::ostringstream jllOut;
        sed::assets::writeJLH(jlhOut, original);
        sed::assets::writeJLL(jllOut, original);

        std::istringstream jlhIn(jlhOut.str());
        std::istringstream jllIn(jllOut.str());
        const sed::assets::JlhTree roundTrip = sed::assets::loadFromJLH(jlhIn, jllIn);

        check(roundTrip.roots.size() == 1, "JLH.nested: root survives save/load");
        if (roundTrip.roots.size() == 1)
        {
            const sed::assets::JlhNode& root = roundTrip.roots[0];
            check(root.label == "Sounds" && root.children.size() == 1,
                "JLH.nested: structural root survives save/load");
            if (root.children.size() == 1)
            {
                const sed::assets::JlhNode& branch = root.children[0];
                check(branch.label == "Gameplay" && branch.children.size() == 1,
                    "JLH.nested: nested structural category survives save/load");
                if (branch.children.size() == 1)
                {
                    const sed::assets::JlhNode& leaf = branch.children[0];
                    check(leaf.label == "Ambience" && leaf.list && leaf.list->entries.size() == 2,
                        "JLH.nested: assignable sound leaf and membership survive save/load");
                }
            }
        }
    }

    {
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root = "tmp_jlh_pair_save_" + unique;
        const std::filesystem::path jlhPath = root / "categories.jlh";
        const std::filesystem::path jllPath = root / "categories.jll";
        std::error_code ec{};
        std::filesystem::create_directories(root, ec);
        check(!ec, "JLH.pair.create test directory");

        {
            std::ofstream oldJlh(jlhPath);
            std::ofstream oldJll(jllPath);
            oldJlh << "SUB Old\nSUBEND\n";
            oldJll << "[Old]\nold.mat\n";
        }

        auto list = std::make_shared<sed::assets::JllList>();
        list->name = "Current";
        list->entries = { "one.mat", "two.mat" };
        sed::assets::JlhTree replacement{};
        replacement.roots.push_back(sed::assets::JlhNode{ "Visible", list, {} });

        bool saveSucceeded = false;
        try
        {
            sed::assets::saveToJLH(replacement, jlhPath);
            saveSucceeded = true;
        }
        catch (...)
        {
        }
        check(saveSucceeded, "JLH.pair checked transactional save succeeds");

        if (saveSucceeded)
        {
            const sed::assets::JlhTree loaded = sed::assets::loadFromJLH(jlhPath);
            check(
                loaded.roots.size() == 1 && loaded.roots[0].label == "Visible"
                    && loaded.roots[0].list && loaded.roots[0].list->name == "Current"
                    && loaded.roots[0].list->entries == std::vector<std::string>{ "one.mat", "two.mat" },
                "JLH.pair replacement hierarchy and membership load as one pair");

            bool transactionArtifactFound = false;
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root))
            {
                transactionArtifactFound = transactionArtifactFound
                    || entry.path().filename().string().find(".sed-pair-save-") != std::string::npos;
            }
            check(!transactionArtifactFound, "JLH.pair successful save removes transaction backups");
        }

        std::filesystem::remove_all(root, ec);
    }

    {
        // A malformed JLL destination must fail preflight without truncating or
        // moving the valid JLH original. This is platform-independent and also
        // protects against accidentally replacing a category directory.
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root = "tmp_jlh_pair_failure_" + unique;
        const std::filesystem::path jlhPath = root / "categories.jlh";
        const std::filesystem::path jllPath = root / "categories.jll";
        const std::filesystem::path sentinelPath = jllPath / "sentinel.txt";
        std::error_code ec{};
        std::filesystem::create_directories(jllPath, ec);
        check(!ec, "JLH.pair.failure create invalid JLL directory");

        {
            std::ofstream oldJlh(jlhPath, std::ios::binary);
            oldJlh << "ORIGINAL JLH\n";
            std::ofstream sentinel(sentinelPath, std::ios::binary);
            sentinel << "KEEP\n";
        }

        sed::assets::JlhTree replacement{};
        bool rejected = false;
        try
        {
            sed::assets::saveToJLH(replacement, jlhPath);
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        check(rejected, "JLH.pair.failure rejects non-file JLL target");

        {
            std::ifstream preservedJlh(jlhPath, std::ios::binary);
            std::ostringstream preservedJlhText;
            preservedJlhText << preservedJlh.rdbuf();
            check(
                preservedJlhText.str() == "ORIGINAL JLH\n",
                "JLH.pair.failure preserves JLH original byte-for-byte");
        }
        check(
            std::filesystem::is_directory(jllPath) && std::filesystem::is_regular_file(sentinelPath),
            "JLH.pair.failure preserves invalid second target and its contents");

        bool transactionArtifactFound = false;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root))
        {
            transactionArtifactFound = transactionArtifactFound
                || entry.path().filename().string().find(".sed-pair-save-") != std::string::npos;
        }
        check(!transactionArtifactFound, "JLH.pair.failure leaves no transaction artifact before staging");

        std::filesystem::remove_all(root, ec);
    }
}

} // namespace sed::tests
