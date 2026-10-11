#include "test_common.hpp"

#include "sed/assets/models_dat.hpp"

#include <filesystem>
#include <sstream>

namespace sed::tests
{

void runModelsDatTests()
{
    const char* const modelsDat = R"MDL(
# comment
0 model0.3do sound0.wav
1 model1.3do
2
)MDL";

    std::istringstream iss(modelsDat);
    const sed::assets::ModelsDatFile file = sed::assets::loadFromModelsDat(iss);

    check(file.a3dos.size() == 2, "MODELS.DAT: a3dos.size");
    if (file.a3dos.size() == 2)
    {
        check(file.a3dos[0] == "model0.3do", "MODELS.DAT: a3dos[0]");
        check(file.a3dos[1] == "model1.3do", "MODELS.DAT: a3dos[1]");
    }

    check(file.snds.size() == 1, "MODELS.DAT: snds.size");
    if (file.snds.size() == 1)
    {
        check(file.snds[0] == "sound0.wav", "MODELS.DAT: snds[0]");
    }

    checkThrowsWithMessage(
        []()
        {
            (void)sed::assets::loadFromModelsDat(
                std::filesystem::path("build/test-temp/core/missing-models.dat"));
        },
        "Failed to open MODELS.DAT: build/test-temp/core/missing-models.dat",
        "MODELS.DAT: missing path reports the failed asset kind");
}

} // namespace sed::tests
