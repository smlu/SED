#include "test_common.hpp"

#include "sed/assets/key.hpp"

#include <filesystem>
#include <sstream>

namespace sed::tests
{

void runKeyTests()
{
    const char* key =
        R"KEY(
            SECTION: HEADER
            FLAGS 0x10
            FRAMES 100
            FPS 30

            SECTION: KEYFRAME
            NODES 1
            NODE 0
            MESH NAME mesh0
            ENTRIES 1
            FRAME 0 0x1 1 2 3 10 20 30
            0.1 0.2 0.3 1 2 3
        )KEY";

    std::istringstream iss(key);
    const sed::assets::KeyFile kf = sed::assets::loadFromKEY(iss);

    check(kf.flags == 0x10u, "KEY: flags");
    check(kf.frames == 100, "KEY: frames");
    check(kf.fps == 30, "KEY: fps");
    check(kf.nodes.size() == 1, "KEY: nodes");
    check(kf.nodes.size() == 1 && kf.nodes[0].meshName == "mesh0", "KEY: mesh name");
    check(kf.nodes.size() == 1 && kf.nodes[0].entries.size() == 1, "KEY: entries");

    if (!kf.nodes.empty() && !kf.nodes[0].entries.empty())
    {
        const auto& entry = kf.nodes[0].entries[0];
        check(entry.label == "FRAME", "KEY: entry label");
        check(entry.frame == 0, "KEY: entry frame");
        check(entry.flags == 1u, "KEY: entry flags");
        check(closeVec(entry.position, sed::TVector{ 1.0, 2.0, 3.0 }), "KEY: entry position");
        check(closeVec(entry.rotation, sed::TVector{ 10.0, 20.0, 30.0 }), "KEY: entry rotation");
        check(closeVec(entry.dPosition, sed::TVector{ 0.1, 0.2, 0.3 }, kRotationEpsilon), "KEY: entry dPosition");
        check(closeVec(entry.dRotation, sed::TVector{ 1.0, 2.0, 3.0 }), "KEY: entry dRotation");
    }

    {
        const char* const oversized = R"KEY(
            SECTION: KEYFRAME
            NODES 2147483647
        )KEY";
        std::istringstream oversizedInput(oversized);
        const sed::assets::KeyFile oversizedFile =
            sed::assets::loadFromKEY(oversizedInput);
        check(oversizedFile.nodes.empty(),
            "KEY: declared node count is bounded by available input");
    }

    {
        const char* const truncated = R"KEY(
            SECTION: HEADER
            FLAGS not_hex
            FRAMES not_an_integer
            FPS 60
            SECTION: KEYFRAME
            NODES 1
            NODE 0
            MESH NAME mesh0
            ENTRIES 2147483647
        )KEY";
        std::istringstream truncatedInput(truncated);
        const sed::assets::KeyFile truncatedFile =
            sed::assets::loadFromKEY(truncatedInput);
        check(truncatedFile.flags == 0
                && truncatedFile.frames == 0
                && truncatedFile.fps == 60,
            "KEY: malformed optional header values use documented defaults");
        check(truncatedFile.nodes.size() == 1
                && truncatedFile.nodes[0].entries.empty(),
            "KEY: declared entry count is bounded by available input");
    }

    checkThrowsWithMessage(
        []()
        {
            (void)sed::assets::loadFromKEY(
                std::filesystem::path("build/test-temp/core/missing.key"));
        },
        "Failed to open KEY file.",
        "KEY: missing path reports an actionable open failure");
}

} // namespace sed::tests
