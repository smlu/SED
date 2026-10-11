#include "test_common.hpp"

#include "sed/assets/spr.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace sed::tests
{
namespace
{

std::string joinWords(const std::vector<std::string>& words)
{
    std::string result;
    for ( const std::string& word : words )
    {
        if ( !result.empty() )
        {
            result.push_back(' ');
        }
        result += word;
    }
    return result;
}

bool loadSprThrows(const std::string& text)
{
    try
    {
        std::istringstream input(text);
        (void)assets::loadFromSPR(input);
        return false;
    }
    catch ( const std::runtime_error& )
    {
        return true;
    }
}

bool saveSprThrows(const assets::SprFile& spr, const assets::SprProfile profile)
{
    try
    {
        (void)assets::saveToSPRText(spr, profile);
        return false;
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
}

bool sameFloatBits(const float first, const float second)
{
    return std::bit_cast<std::uint32_t>(first) == std::bit_cast<std::uint32_t>(second);
}

struct CommaDecimal final : std::numpunct<char>
{
    char do_decimal_point() const override
    {
        return ',';
    }
};

class ScopedGlobalLocale final
{
public:
    explicit ScopedGlobalLocale(const std::locale& locale)
        : original_(std::locale::global(locale))
    {
    }

    ~ScopedGlobalLocale()
    {
        std::locale::global(original_);
    }

private:
    std::locale original_;
};

} // namespace

void runSprTests()
{
    {
        std::istringstream input(R"SPR(
# ignored comment
; another ignored comment
// one more ignored comment

sprite.mat 2 3.25 4.5 5 6 7 0.125 8.25 -9.5 10.75 ; trailing comment
)SPR");
        const assets::SprFile spr = assets::loadFromSPR(input);

        check(spr.material == "sprite.mat",
            "SPR classic: material is parsed");
        check(spr.type == 2,
            "SPR classic: type is parsed");
        check(close(spr.width, 3.25),
            "SPR classic: width is parsed");
        check(close(spr.height, 4.5),
            "SPR classic: height is parsed");
        check(spr.geometryMode == 5,
            "SPR classic: geometry mode is parsed");
        check(spr.lightMode == 6,
            "SPR classic: light mode is parsed");
        check(spr.textureMode == 7,
            "SPR classic: texture mode is parsed");
        check(close(spr.extraLightRGBA[0], 0.125),
            "SPR classic: scalar extra light expands to red");
        check(close(spr.extraLightRGBA[1], 0.125),
            "SPR classic: scalar extra light expands to green");
        check(close(spr.extraLightRGBA[2], 0.125),
            "SPR classic: scalar extra light expands to blue");
        check(close(spr.extraLightRGBA[3], 1.0),
            "SPR classic: scalar extra light sets opaque alpha");
        check(close(spr.offsetXYZ[0], 8.25),
            "SPR classic: X offset is parsed");
        check(close(spr.offsetXYZ[1], -9.5),
            "SPR classic: Y offset is parsed");
        check(close(spr.offsetXYZ[2], 10.75),
            "SPR classic: Z offset is parsed");
    }

    {
        std::istringstream input(
            "ijim.mat 1 2.0 3.0 4 5 6 0.1 0.2 0.3 0.4 -7.0 8.0 9.0");
        const assets::SprFile spr = assets::loadFromSPR(input);

        check(spr.material == "ijim.mat",
            "SPR IJIM: material is parsed");
        check(spr.type == 1,
            "SPR IJIM: type is parsed");
        check(close(spr.width, 2.0),
            "SPR IJIM: width is parsed");
        check(close(spr.height, 3.0),
            "SPR IJIM: height is parsed");
        check(spr.geometryMode == 4,
            "SPR IJIM: geometry mode is parsed");
        check(spr.lightMode == 5,
            "SPR IJIM: light mode is parsed");
        check(spr.textureMode == 6,
            "SPR IJIM: texture mode is parsed");
        check(close(spr.extraLightRGBA[0], 0.1),
            "SPR IJIM: red extra light is parsed");
        check(close(spr.extraLightRGBA[1], 0.2),
            "SPR IJIM: green extra light is parsed");
        check(close(spr.extraLightRGBA[2], 0.3),
            "SPR IJIM: blue extra light is parsed");
        check(close(spr.extraLightRGBA[3], 0.4),
            "SPR IJIM: alpha extra light is parsed");
        check(close(spr.offsetXYZ[0], -7.0),
            "SPR IJIM: X offset is parsed");
        check(close(spr.offsetXYZ[1], 8.0),
            "SPR IJIM: Y offset is parsed");
        check(close(spr.offsetXYZ[2], 9.0),
            "SPR IJIM: Z offset is parsed");
    }

    {
        const std::array<const char*, 3> extraFieldRecords{
            "sprite.mat 1 2 3 4 5 6 0.25 7 8 9 ignored",
            "sprite.mat 1 2 3 4 5 6 0.25 7 8 9 ignored ignored",
            "sprite.mat 1 2 3 4 5 6 0.25 7 8 9 a b c d e",
        };
        for ( const char* const record : extraFieldRecords )
        {
            std::istringstream input(record);
            const assets::SprFile spr = assets::loadFromSPR(input);

            check(spr.material == "sprite.mat",
                "SPR field count: 12, 13, and greater-than-14 records use the classic prefix");
            check(close(spr.offsetXYZ[2], 9.0),
                "SPR field count: classic extra fields do not change parsed offsets");
        }

        check(loadSprThrows("sprite.mat 1 2 3 4 5 6 7 8 9"),
            "SPR field count: fewer than 11 fields are rejected");
        check(loadSprThrows(""),
            "SPR field count: empty input is rejected");
        check(loadSprThrows("# comment only\n ; comment only\n"),
            "SPR field count: comment-only input is rejected");
    }

    {
        const std::vector<std::string> classic{
            "sprite.mat", "1", "2", "3", "4", "5", "6", "0.25", "7", "8", "9",
        };
        for ( std::size_t field = 1U; field < classic.size(); ++field )
        {
            std::vector<std::string> malformed = classic;
            malformed[field] = "bad";
            check(loadSprThrows(joinWords(malformed)),
                "SPR classic numeric validation: every malformed numeric field is rejected");
        }

        const std::vector<std::string> ijim{
            "ijim.mat", "1", "2", "3", "4", "5", "6",
            "0.1", "0.2", "0.3", "0.4", "7", "8", "9",
        };
        for ( std::size_t field = 1U; field < ijim.size(); ++field )
        {
            std::vector<std::string> malformed = ijim;
            malformed[field] = "bad";
            check(loadSprThrows(joinWords(malformed)),
                "SPR IJIM numeric validation: every malformed numeric field is rejected");
        }

        check(loadSprThrows("sprite.mat 1x 2 3 4 5 6 0.25 7 8 9"),
            "SPR integer validation: trailing junk is rejected");
        check(loadSprThrows("sprite.mat 1 2x 3 4 5 6 0.25 7 8 9"),
            "SPR float validation: trailing junk is rejected");
        check(loadSprThrows("sprite.mat 999999999999999999999 2 3 4 5 6 0.25 7 8 9"),
            "SPR integer validation: overflow is rejected");
        check(loadSprThrows("sprite.mat 1 1e999 3 4 5 6 0.25 7 8 9"),
            "SPR float validation: overflow is rejected");
        check(loadSprThrows("sprite.mat 1 nan 3 4 5 6 0.25 7 8 9"),
            "SPR float validation: NaN is rejected");
        check(loadSprThrows("sprite.mat 1 inf 3 4 5 6 0.25 7 8 9"),
            "SPR float validation: positive infinity is rejected");
        check(loadSprThrows("sprite.mat 1 -inf 3 4 5 6 0.25 7 8 9"),
            "SPR float validation: negative infinity is rejected");
    }

    {
        std::istringstream input(R"SPR(
# comment
test.mat ; scanner ignores all other fields
other.mat
)SPR");
        check(assets::matFromSPR(input) == "test.mat",
            "SPR material scanner: first noncomment token is returned");

        std::istringstream empty("");
        check(assets::matFromSPR(empty).empty(),
            "SPR material scanner: empty input returns no material");

        std::istringstream comments("# comment\n; comment\n// comment\n");
        check(assets::matFromSPR(comments).empty(),
            "SPR material scanner: comment-only input returns no material");
    }

    {
        const std::filesystem::path path = std::filesystem::temp_directory_path()
            / "sed_core_spr_parser_test.spr";
        std::error_code error;
        std::filesystem::remove(path, error);

        {
            std::ofstream output(path);
            output << "path.mat 1 2 3 4 5 6 0.5 7 8 9\n";
        }

        const assets::SprFile spr = assets::loadFromSPR(path);
        check(spr.material == "path.mat",
            "SPR path: full parser reads an existing file");
        check(assets::matFromSPR(path) == "path.mat",
            "SPR path: lightweight scanner reads an existing file");

        std::filesystem::remove(path, error);

        bool loadMissingThrows = false;
        try
        {
            (void)assets::loadFromSPR(path);
        }
        catch ( const std::runtime_error& )
        {
            loadMissingThrows = true;
        }
        check(loadMissingThrows,
            "SPR path: full parser reports a missing file");

        bool scanMissingThrows = false;
        try
        {
            (void)assets::matFromSPR(path);
        }
        catch ( const std::runtime_error& )
        {
            scanMissingThrows = true;
        }
        check(scanMissingThrows,
            "SPR path: material scanner reports a missing file");
    }

    {
        assets::SprFile spr{};
        spr.material = "sprite.mat";
        spr.type = 2;
        spr.width = 3.25f;
        spr.height = 4.5f;
        spr.geometryMode = 5;
        spr.lightMode = 6;
        spr.textureMode = 7;
        spr.extraLightRGBA = { 0.125f, 0.125f, 0.125f, 1.0f };
        spr.offsetXYZ = { 8.25f, -9.5f, 10.75f };

        constexpr std::string_view expected =
            "sprite.mat 2 3.25 4.5 5 6 7 0.125 8.25 -9.5 10.75\n";
        check(assets::saveToSPRText(spr, assets::SprProfile::classic) == expected,
            "SPR authoring: classic profile matches the complete fixed text vector");

        std::ostringstream output;
        output << "prefix\n";
        assets::saveToSPRText(output, spr, assets::SprProfile::classic);
        check(output.str() == std::string("prefix\n") + std::string(expected),
            "SPR authoring: stream writer appends the exact deterministic text");

        std::istringstream roundTrip{ std::string(expected) };
        const assets::SprFile loaded = assets::loadFromSPR(roundTrip);
        check(loaded.material == spr.material
                && loaded.type == spr.type
                && loaded.width == spr.width
                && loaded.height == spr.height
                && loaded.geometryMode == spr.geometryMode
                && loaded.lightMode == spr.lightMode
                && loaded.textureMode == spr.textureMode
                && loaded.extraLightRGBA == spr.extraLightRGBA
                && loaded.offsetXYZ == spr.offsetXYZ,
            "SPR authoring: classic save/load preserves every scalar, color, and offset field");
    }

    {
        assets::SprFile spr{};
        spr.material = "ijim.mat";
        spr.type = 1;
        spr.width = 2.0f;
        spr.height = 3.0f;
        spr.geometryMode = 4;
        spr.lightMode = 5;
        spr.textureMode = 6;
        spr.extraLightRGBA = { 0.125f, 0.25f, 0.375f, 0.5f };
        spr.offsetXYZ = { -0.625f, 0.75f, 0.875f };

        constexpr std::string_view expected =
            "ijim.mat 1 2 3 4 5 6 0.125 0.25 0.375 0.5 -0.625 0.75 0.875\n";
        check(assets::saveToSPRText(spr, assets::SprProfile::ijim) == expected,
            "SPR authoring: IJIM profile matches the complete RGBA and offset vector");

        std::istringstream roundTrip{ std::string(expected) };
        const assets::SprFile loaded = assets::loadFromSPR(roundTrip);
        check(loaded.material == spr.material
                && loaded.type == spr.type
                && loaded.width == spr.width
                && loaded.height == spr.height
                && loaded.geometryMode == spr.geometryMode
                && loaded.lightMode == spr.lightMode
                && loaded.textureMode == spr.textureMode
                && loaded.extraLightRGBA == spr.extraLightRGBA
                && loaded.offsetXYZ == spr.offsetXYZ,
            "SPR authoring: IJIM save/load preserves all RGBA and offset fields");

        spr.width = std::nextafter(1.0f, 2.0f);
        spr.extraLightRGBA[0] = std::nextafter(0.1f, 1.0f);
        spr.extraLightRGBA[1] = spr.extraLightRGBA[0];
        spr.extraLightRGBA[2] = spr.extraLightRGBA[0];
        spr.extraLightRGBA[3] = 1.0f;
        spr.offsetXYZ[0] = std::nextafter(-1.0f, -2.0f);
        spr.offsetXYZ[1] = std::nextafter(0.5f, 1.0f);
        spr.offsetXYZ[2] = std::nextafter(0.25f, 0.0f);
        const std::string preciseText =
            assets::saveToSPRText(spr, assets::SprProfile::classic);
        bool preciseRoundTripPreserved = false;
        try
        {
            std::istringstream preciseRoundTrip(preciseText);
            const assets::SprFile preciseLoaded = assets::loadFromSPR(preciseRoundTrip);
            preciseRoundTripPreserved = sameFloatBits(preciseLoaded.width, spr.width)
                && sameFloatBits(preciseLoaded.extraLightRGBA[0], spr.extraLightRGBA[0])
                && sameFloatBits(preciseLoaded.offsetXYZ[0], spr.offsetXYZ[0])
                && sameFloatBits(preciseLoaded.offsetXYZ[1], spr.offsetXYZ[1])
                && sameFloatBits(preciseLoaded.offsetXYZ[2], spr.offsetXYZ[2]);
        }
        catch (const std::runtime_error&)
        {
        }
        check(preciseRoundTripPreserved,
            "SPR authoring: emitted float precision round-trips stored float bits");
    }

    {
        assets::SprFile spr{};
        spr.material = "locale.mat";
        spr.width = 3.25f;
        spr.height = 4.5f;
        spr.extraLightRGBA = { 0.125f, 0.125f, 0.125f, 1.0f };
        spr.offsetXYZ = { 8.25f, -9.5f, 10.75f };
        const ScopedGlobalLocale locale(
            std::locale(std::locale::classic(), new CommaDecimal));
        (void)locale;
        const std::string localized =
            assets::saveToSPRText(spr, assets::SprProfile::classic);
        check(localized == "locale.mat 0 3.25 4.5 0 0 0 0.125 8.25 -9.5 10.75\n",
            "SPR authoring: decimal output ignores the process numeric locale");
    }

    {
        struct MaterialBoundary
        {
            assets::SprProfile profile{};
            std::size_t maximumBytes{};
            std::string_view suffix{};
        };
        const std::array<MaterialBoundary, 2> boundaries{
            MaterialBoundary{ assets::SprProfile::classic, 31U, " 2 1 2 4 3 1 0.25 -1 2 -3\n" },
            MaterialBoundary{ assets::SprProfile::ijim, 63U, " 2 1 2 4 3 1 0.25 0.25 0.25 1 -1 2 -3\n" } };
        for ( const MaterialBoundary& boundary : boundaries )
        {
            assets::SprFile sprite{};
            sprite.material = std::string(boundary.maximumBytes - 4U, 'm') + ".mat";
            sprite.type = 2;
            sprite.width = 1.0f;
            sprite.height = 2.0f;
            sprite.geometryMode = 4;
            sprite.lightMode = 3;
            sprite.textureMode = 1;
            sprite.extraLightRGBA = { 0.25f, 0.25f, 0.25f, 1.0f };
            sprite.offsetXYZ = { -1.0f, 2.0f, -3.0f };
            check(assets::saveToSPRText(sprite, boundary.profile)
                    == sprite.material + std::string(boundary.suffix),
                "SPR engine boundary: maximum material name writes every authored field without truncation");

            sprite.material.insert(sprite.material.begin(), 'm');
            const std::string rawSource = sprite.material + std::string(boundary.suffix);
            std::istringstream rawInput(rawSource);
            const assets::SprFile retained = assets::loadFromSPR(rawInput);
            check(retained.material == sprite.material && retained.offsetXYZ == sprite.offsetXYZ
                    && retained.textureMode == sprite.textureMode,
                "SPR engine boundary: overlong engine material names remain exact in imported raw drafts");
            check(saveSprThrows(retained, boundary.profile),
                "SPR engine boundary: external writer rejects a material name one byte beyond its profile buffer");
            std::ostringstream unchanged;
            unchanged << "sentinel";
            try
            {
                assets::saveToSPRText(unchanged, retained, boundary.profile);
            }
            catch ( const std::runtime_error& )
            {
            }
            check(unchanged.str() == "sentinel",
                "SPR engine boundary: rejected material identity cannot partially append external bytes");
        }
    }

    {
        struct RawRecordShape
        {
            const char* const label;
            const std::string_view suffix;
        };
        constexpr std::array<RawRecordShape, 2> recordShapes{
            RawRecordShape{ "classic", " 1 2 3 4 5 6 0.25 7 8 9" },
            RawRecordShape{ "IJIM", " 1 2 3 4 5 6 0.1 0.2 0.3 0.4 7 8 9" },
        };

        constexpr std::string_view pathPrefix = "textures\\nested/";
        const std::string maximumRawMaterial = std::string(pathPrefix)
            + std::string(255U - pathPrefix.size(), 'm');
        check(maximumRawMaterial.size() == 255U,
            "SPR raw import: boundary fixture is exactly 255 bytes");

        for (const RawRecordShape& shape : recordShapes)
        {
            std::istringstream acceptedInput(maximumRawMaterial + std::string(shape.suffix));
            bool accepted = false;
            try
            {
                accepted = assets::loadFromSPR(acceptedInput).material == maximumRawMaterial;
            }
            catch (const std::runtime_error&)
            {
            }
            const std::string acceptedMessage = std::string("SPR raw import: ") + shape.label
                + " accepts a safe 255-byte path/name token independent of engine writer limits";
            check(accepted, acceptedMessage.c_str());

            const std::string rejectedMessage = std::string("SPR raw import: ") + shape.label
                + " rejects a 256-byte material token";
            check(loadSprThrows(maximumRawMaterial + "m" + std::string(shape.suffix)),
                rejectedMessage.c_str());
        }

        std::vector<std::string> unsafeMaterials{
            "two words.mat",
            "bad#comment.mat",
            "bad;comment.mat",
            "bad//comment.mat",
            "bad,comma.mat",
            "bad=token.mat",
            "bad\"quote.mat",
            std::string("bad") + '\t' + "word.mat",
            std::string("bad") + static_cast<char>(0x01) + "control.mat",
            std::string("bad") + static_cast<char>(0x7F) + "delete.mat",
            std::string("bad\0nul.mat", 11U),
            "bad\nline.mat",
        };
        for (const RawRecordShape& shape : recordShapes)
        {
            for (const std::string& material : unsafeMaterials)
            {
                const std::string message = std::string("SPR raw import: ") + shape.label
                    + " rejects unsafe material token bytes";
                check(loadSprThrows(material + std::string(shape.suffix)), message.c_str());
            }
        }
    }

    {
        std::ostringstream unchanged;
        unchanged << "sentinel";
        checkThrowsWithMessage(
            [&unchanged]()
            {
                assets::SprFile sprite{};
                sprite.material = "unknown.mat";
                assets::saveToSPRText(unchanged, sprite, static_cast<assets::SprProfile>(255U));
            },
            "Unknown SPR profile.",
            "SPR engine boundary: stream writer rejects an unknown engine profile");
        check(unchanged.str() == "sentinel",
            "SPR engine boundary: an unknown engine profile leaves stream output unchanged");
    }

    {
        assets::SprFile valid{};
        valid.material = "valid.mat";
        valid.width = 1.0f;
        valid.height = 1.0f;

        assets::SprFile colored = valid;
        colored.extraLightRGBA = { 0.1f, 0.2f, 0.3f, 1.0f };
        check(saveSprThrows(colored, assets::SprProfile::classic),
            "SPR authoring: classic profile rejects colored IJIM data instead of flattening RGB");
        colored.extraLightRGBA = { 0.1f, 0.1f, 0.1f, 0.75f };
        check(saveSprThrows(colored, assets::SprProfile::classic),
            "SPR authoring: classic profile rejects non-opaque alpha instead of discarding it");

        const std::array<std::string, 9> unsafeMaterials{
            "", "two words.mat", "bad#comment.mat", "bad;comment.mat", "bad//comment.mat",
            "comma,name.mat", "equals=name.mat", "quoted\"name.mat", "control\x01name.mat",
        };
        bool allUnsafeRejected = true;
        for (const std::string& material : unsafeMaterials)
        {
            assets::SprFile unsafe = valid;
            unsafe.material = material;
            allUnsafeRejected = allUnsafeRejected
                && saveSprThrows(unsafe, assets::SprProfile::ijim);
        }
        assets::SprFile newline = valid;
        newline.material = "good.mat\nnext.mat";
        allUnsafeRejected = allUnsafeRejected
            && saveSprThrows(newline, assets::SprProfile::ijim);
        std::string embeddedNul("good.mat\0tail", 13);
        assets::SprFile nulToken = valid;
        nulToken.material = std::move(embeddedNul);
        allUnsafeRejected = allUnsafeRejected
            && saveSprThrows(nulToken, assets::SprProfile::ijim);
        check(allUnsafeRejected,
            "SPR authoring: rejects empty, whitespace, comment, newline, and NUL resource-token injection");

        bool allNonFiniteRejected = true;
        for (std::size_t index = 0; index < 9U; ++index)
        {
            assets::SprFile invalid = valid;
            float* const invalidField = index < 2
                ? (index == 0 ? &invalid.width : &invalid.height)
                : index < 6
                    ? &invalid.extraLightRGBA[index - 2]
                    : &invalid.offsetXYZ[index - 6];
            *invalidField = std::numeric_limits<float>::quiet_NaN();
            allNonFiniteRejected = allNonFiniteRejected
                && saveSprThrows(invalid, assets::SprProfile::ijim);
        }
        assets::SprFile infinity = valid;
        infinity.offsetXYZ[2] = std::numeric_limits<float>::infinity();
        allNonFiniteRejected = allNonFiniteRejected
            && saveSprThrows(infinity, assets::SprProfile::ijim);
        check(allNonFiniteRejected,
            "SPR authoring: validates every dimension, RGBA component, and offset for finite values");

        std::ostringstream unchanged;
        unchanged << "sentinel";
        try
        {
            assets::saveToSPRText(unchanged, colored, assets::SprProfile::classic);
        }
        catch (const std::runtime_error&)
        {
        }
        check(unchanged.str() == "sentinel",
            "SPR authoring: validation failure does not partially append output");

        std::ostream failedOutput(nullptr);
        bool streamFailureRejected = false;
        try
        {
            assets::saveToSPRText(failedOutput, valid, assets::SprProfile::ijim);
        }
        catch (const std::runtime_error&)
        {
            streamFailureRejected = true;
        }
        check(streamFailureRejected && failedOutput.fail(),
            "SPR authoring: stream device failure is reported and retained in stream state");
    }

    {
        class FailedReadBuffer final : public std::streambuf
        {
        protected:
            int_type underflow() override
            {
                throw std::runtime_error("synthetic input device failure");
            }
        } buffer;

        std::istream failedInput(&buffer);
        bool rejected = false;
        try
        {
            (void)assets::loadFromSPR(failedInput);
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        check(rejected && failedInput.bad(),
            "SPR authoring: source stream device failure is reported");
    }
}

} // namespace sed::tests
