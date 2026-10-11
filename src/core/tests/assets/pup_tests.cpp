#include "test_common.hpp"

#include "sed/assets/pup.hpp"

#include <filesystem>
#include <sstream>

namespace sed::tests
{

void runPupTests()
{
    const char* const pup = R"PUP(
# comment
walk walk.key
shoot SHOOT.KEY ; comment
noop not_a_key.txt
singleword
)PUP";

    std::istringstream iss(pup);
    const sed::assets::PupFile file = sed::assets::loadFromPUP(iss);

    check(file.keys.size() == 2, "PUP: keys.size");
    if (file.keys.size() == 2)
    {
        check(file.keys[0] == "walk.key", "PUP: keys[0]");
        check(file.keys[1] == "SHOOT.KEY", "PUP: keys[1]");
    }

    checkThrowsWithMessage(
        []()
        {
            (void)sed::assets::loadFromPUP(
                std::filesystem::path("build/test-temp/core/missing.pup"));
        },
        "Failed to open PUP: build/test-temp/core/missing.pup",
        "PUP: missing path reports the failed asset kind");
}

} // namespace sed::tests
