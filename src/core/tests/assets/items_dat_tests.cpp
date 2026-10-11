#include "test_common.hpp"

#include "sed/assets/items_dat.hpp"

#include <filesystem>
#include <sstream>

namespace sed::tests
{

void runItemsDatTests()
{
    const char* const itemsDat = R"ITM(
# comment
0 cog=test.cog other=1
1 other=2 cog=Foo.COG
2 other=3
3 notcog=wrong.cog
4 cog=   spaced.cog trailing=1
)ITM";

    std::istringstream iss(itemsDat);
    const sed::assets::ItemsDatFile file = sed::assets::loadFromItemsDat(iss);

    check(file.cogs.size() == 3, "ITEMS.DAT: cogs.size");
    if (file.cogs.size() == 3)
    {
        check(file.cogs[0] == "test.cog", "ITEMS.DAT: cogs[0]");
        check(file.cogs[1] == "Foo.COG", "ITEMS.DAT: cogs[1]");
        check(file.cogs[2] == "spaced.cog", "ITEMS.DAT: cogs[2]");
    }

    checkThrowsWithMessage(
        []()
        {
            (void)sed::assets::loadFromItemsDat(
                std::filesystem::path("build/test-temp/core/missing-items.dat"));
        },
        "Failed to open ITEMS.DAT: build/test-temp/core/missing-items.dat",
        "ITEMS.DAT: missing path reports the failed asset kind");
}

} // namespace sed::tests
