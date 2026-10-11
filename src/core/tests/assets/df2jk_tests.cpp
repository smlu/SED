#include "test_common.hpp"

#include "sed/assets/df2jk.hpp"

#include <sstream>

namespace sed::tests
{

void runDf2JkTests()
{
    const char* input = R"LST(
    MORTAR      railgun        # Mortar->Railgun
    player      walkplayer
    INVALID_ONLY_ONE_TOKEN
    )LST";

    std::istringstream in(input);
    const sed::assets::Df2JkConvTable table = sed::assets::loadDf2JkConversionTable(in);

    check(table.size() == 2, "df2jk: table.size");

    const std::optional<std::string_view> mortar = sed::assets::lookupDf2Jk(table, "mortar");
    check(mortar.has_value(), "df2jk: lookup mortar");
    check(mortar && *mortar == "railgun", "df2jk: mortar=railgun");

    const std::optional<std::string_view> player = sed::assets::lookupDf2Jk(table, "PLAYER");
    check(player.has_value(), "df2jk: lookup player");
    check(player && *player == "walkplayer", "df2jk: player=walkplayer");

    const std::optional<std::string_view> unknown = sed::assets::lookupDf2Jk(table, "UNKNOWN");
    check(!unknown.has_value(), "df2jk: unknown empty");
}

} // namespace sed::tests
