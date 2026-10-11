#include "test_common.hpp"

#include "sed/assets/par.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace sed::tests
{

void runParTests()
{
    {
        std::istringstream input(R"PAR(
# comment
; comment
// comment
FOO bar
MATERIAL
material lowercase.mat
  MATERIAL   first.mat   ignored-value // trailing comment
MATERIAL second.mat
)PAR");
        check(assets::matFromPAR(input) == "first.mat",
            "PAR scanner: the first valid uppercase MATERIAL declaration wins");
    }

    {
        std::istringstream absent("FOO bar\nSIZE 1\n");
        check(assets::matFromPAR(absent).empty(),
            "PAR scanner: an absent material declaration returns empty");

        std::istringstream lowercase("material lowercase.mat\n");
        check(assets::matFromPAR(lowercase).empty(),
            "PAR scanner: lowercase material is not an uppercase declaration");

        std::istringstream malformed("MATERIAL\nMATERIAL valid.mat\n");
        check(assets::matFromPAR(malformed) == "valid.mat",
            "PAR scanner: a missing material value is skipped");

        std::istringstream commented(
            "MATERIAL # no value\nMATERIAL ; no value\nMATERIAL // no value\n");
        check(assets::matFromPAR(commented).empty(),
            "PAR scanner: declarations whose values are comments are skipped");

        std::istringstream empty("");
        check(assets::matFromPAR(empty).empty(),
            "PAR scanner: empty input returns empty");
    }

    {
        const std::filesystem::path path = std::filesystem::temp_directory_path()
            / "sed_core_par_scanner_test.par";
        std::error_code error;
        std::filesystem::remove(path, error);

        {
            std::ofstream output(path);
            output << "MATERIAL path.mat\n";
        }

        check(assets::matFromPAR(path) == "path.mat",
            "PAR path: scanner reads an existing file");

        std::filesystem::remove(path, error);

        bool missingThrows = false;
        try
        {
            (void)assets::matFromPAR(path);
        }
        catch ( const std::runtime_error& )
        {
            missingThrows = true;
        }
        check(missingThrows,
            "PAR path: scanner reports a missing file");
    }
}

} // namespace sed::tests
