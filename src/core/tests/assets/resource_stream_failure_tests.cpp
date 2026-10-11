#include "test_common.hpp"
#include "sed/assets/key.hpp"
#include "sed/assets/pup.hpp"
#include "sed/assets/snd.hpp"
#include "sed/assets/items_dat.hpp"
#include "sed/assets/models_dat.hpp"
#include "sed/assets/spr.hpp"
#include <functional>
#include <clocale>
#include <limits>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace sed::tests
{
namespace
{
assets::SprFile priorSprite()
{
    return assets::SprFile{"unchanged.mat", 71, 72.25f, 73.5f, 74, 75, 76,
        {77.125f, 78.25f, 79.5f, 80.75f}, {81.125f, 82.25f, 83.5f}};
}

bool exactSprite(const assets::SprFile& actual, const assets::SprFile& expected)
{
    return actual.material == expected.material && actual.type == expected.type
        && actual.width == expected.width && actual.height == expected.height
        && actual.geometryMode == expected.geometryMode && actual.lightMode == expected.lightMode
        && actual.textureMode == expected.textureMode && actual.extraLightRGBA == expected.extraLightRGBA
        && actual.offsetXYZ == expected.offsetXYZ;
}

class FailingResourceInput final : public std::streambuf
{
public:
    explicit FailingResourceInput(std::string prefix) : prefix_(std::move(prefix))
    {
        setg(prefix_.data(), prefix_.data(), prefix_.data() + prefix_.size());
    }
private:
    int_type underflow() override { throw std::runtime_error("injected device read failure"); }
    std::string prefix_;
};
}

void runResourceStreamFailureTests()
{
    struct ReadCase
    {
        const char* prefix;
        const char* error;
        std::function<void(std::istream&)> read;
    };
    const ReadCase cases[] = {
        {"SECTION: HEADER\nFLAGS 0x2\nFRAMES 12\nFPS 30\nEND\n# partial", "Failed to read KEY.",
            [](std::istream& in) { (void)assets::loadFromKEY(in); }},
        {"idle anim.key\n# partial", "Failed to read PUP.",
            [](std::istream& in) { (void)assets::loadFromPUP(in); }},
        {"idle sound.wav\n# partial", "Failed to read SND.",
            [](std::istream& in) { (void)assets::loadFromSND(in); }},
        {"id cog=actor.cog\n# partial", "Failed to read ITEMS.DAT.",
            [](std::istream& in) { (void)assets::loadFromItemsDat(in); }},
        {"entry actor.3do actor.snd\n# partial", "Failed to read MODELS.DAT.",
            [](std::istream& in) { (void)assets::loadFromModelsDat(in); }},
        // SPR consumes its first complete data record; fail during that record,
        // preserving the established rule that trailing records are irrelevant.
        {"# prefix\nsprite.mat 0 2 3 4 3 3 0.5 1 2 3", "Failed to read SPR.",
            [](std::istream& in) { (void)assets::loadFromSPR(in); }},
        {"# prefix\nsprite.mat", "Failed to read SPR.",
            [](std::istream& in) { (void)assets::matFromSPR(in); }},
    };
    for (const auto& item : cases)
    {
        FailingResourceInput buffer(item.prefix);
        std::istream input(&buffer);
        bool returned = false;
        std::string message;
        try { item.read(input); returned = true; }
        catch (const std::runtime_error& error) { message = error.what(); }
        check(!returned && input.bad() && message == item.error,
            (std::string("Resource stream device failure rejects partial result with exact diagnostic: ") + item.error).c_str());
    }

    std::istringstream emptyKey, emptyPup, emptySnd, emptyItems, emptyModels, emptyMaterial, emptySprite;
    const auto keyEmpty = assets::loadFromKEY(emptyKey);
    const auto modelsEmpty = assets::loadFromModelsDat(emptyModels);
    check(keyEmpty.flags == 0 && keyEmpty.frames == 0 && keyEmpty.fps == 0 && keyEmpty.nodes.empty()
            && assets::loadFromPUP(emptyPup).keys.empty() && assets::loadFromSND(emptySnd).wavs.empty()
            && assets::loadFromItemsDat(emptyItems).cogs.empty()
            && modelsEmpty.a3dos.empty() && modelsEmpty.snds.empty()
            && assets::matFromSPR(emptyMaterial).empty(),
        "Resource list parsers and material-only SPR preserve their complete empty-input contract");
    bool emptySpriteRejected = false;
    try { (void)assets::loadFromSPR(emptySprite); }
    catch (const std::runtime_error& error) { emptySpriteRejected = std::string(error.what()) == "Empty SPR."; }
    check(emptySpriteRejected, "Full SPR parser rejects empty input with complete fixed diagnostic");

    std::istringstream keyInput("SECTION: HEADER\nFLAGS 0x2\nFRAMES 12\nFPS 30\nEND");
    const auto key = assets::loadFromKEY(keyInput);
    check(key.flags == 2 && key.frames == 12 && key.fps == 30 && key.nodes.empty(),
        "KEY complete fixed final line without newline returns all header fields");
    std::istringstream pupInput("idle anim.key");
    std::istringstream sndInput("idle sound.wav");
    std::istringstream itemsInput("id cog=actor.cog");
    std::istringstream modelsInput("entry actor.3do actor.snd");
    const auto models = assets::loadFromModelsDat(modelsInput);
    check(assets::loadFromPUP(pupInput).keys == std::vector<std::string>{"anim.key"}
            && assets::loadFromSND(sndInput).wavs == std::vector<std::string>{"sound.wav"}
            && assets::loadFromItemsDat(itemsInput).cogs == std::vector<std::string>{"actor.cog"}
            && models.a3dos == std::vector<std::string>{"actor.3do"}
            && models.snds == std::vector<std::string>{"actor.snd"},
        "Resource text loaders preserve complete fixed string vectors on ordinary EOF");
    std::istringstream extensionsInput("0 a\n1 key\n2 .KeY\n3 animation.kEy\n4 animation.key.backup\n5 animation.ke\n6 ANIMATION.KEY");
    check(assets::loadFromPUP(extensionsInput).keys
            == std::vector<std::string>{".KeY", "animation.kEy", "ANIMATION.KEY"},
        "PUP suffix matching preserves exact authored case and rejects short or nonfinal extensions");
    struct IntVector { const char* text; int expected; };
    const IntVector intVectors[] = {
        {"+17", 17}, {"-0", 0}, {"0017", 17},
        {"2147483647", std::numeric_limits<int>::max()},
        {"-2147483648", std::numeric_limits<int>::min()},
    };
    for (const auto& item : intVectors)
    {
        std::istringstream input(std::string("sprite.mat ") + item.text + " 2 3 " + item.text
            + " " + item.text + " " + item.text + " 0.5 1 2 3");
        const auto parsed = assets::loadFromSPR(input);
        check(parsed.type == item.expected && parsed.geometryMode == item.expected
                && parsed.lightMode == item.expected && parsed.textureMode == item.expected
                && parsed.material == "sprite.mat" && parsed.width == 2 && parsed.height == 3
                && parsed.extraLightRGBA == std::array<float,4>{0.5f, 0.5f, 0.5f, 1.0f}
                && parsed.offsetXYZ == std::array<float,3>{1, 2, 3},
            (std::string("SPR fixed integer token preserves every field: ") + item.text).c_str());
    }
    for (const char* invalid : {"2147483648", "-2147483649", "+-1", "++1", "1x", "0x1", "1.0", "+", "-"})
    {
        const auto prior = priorSprite();
        assets::SprFile published = prior;
        bool rejected = false;
        try
        {
            std::istringstream input(std::string("sprite.mat ") + invalid + " 2 3 4 3 3 0 0 0 0");
            published = assets::loadFromSPR(input);
        }
        catch (const std::runtime_error& error)
        {
            rejected = std::string(error.what()) == "Bad numeric data in JKDF2/MotS SPR.";
        }
        check(rejected && exactSprite(published, prior),
            (std::string("SPR invalid integer preserves caller result with exact diagnostic: ") + invalid).c_str());
    }
    std::istringstream sprInput("sprite.mat 0 2 3 4 3 3 0.5 1 2 3");
    const auto sprite = assets::loadFromSPR(sprInput);
    check(sprite.material == "sprite.mat" && sprite.type == 0 && sprite.width == 2 && sprite.height == 3
            && sprite.geometryMode == 4 && sprite.lightMode == 3 && sprite.textureMode == 3
            && sprite.extraLightRGBA == std::array<float,4>{0.5f, 0.5f, 0.5f, 1.0f}
            && sprite.offsetXYZ == std::array<float,3>{1.0f, 2.0f, 3.0f},
        "SPR complete fixed final line returns every numeric and material field");
    struct FloatVector { const char* text; float expected; };
    const FloatVector floatVectors[] = {
        {"+1.25", 1.25f}, {"-0", -0.0f}, {"0x1p+1", 2.0f},
        {"-0X1.8p+1", -3.0f}, {"1.40129846e-45", std::numeric_limits<float>::denorm_min()},
        {"3.4028234663852886e+38", std::numeric_limits<float>::max()},
    };
    for (const auto& item : floatVectors)
    {
        bool matched = false;
        try
        {
            std::istringstream input(std::string("sprite.mat 0 ") + item.text + " 2 4 3 3 0 0 0 0");
            const auto parsed = assets::loadFromSPR(input);
            matched = parsed.width == item.expected && std::signbit(parsed.width) == std::signbit(item.expected)
                && parsed.material == "sprite.mat" && parsed.height == 2 && parsed.type == 0
                && parsed.geometryMode == 4 && parsed.lightMode == 3 && parsed.textureMode == 3
                && parsed.extraLightRGBA == std::array<float,4>{0, 0, 0, 1}
                && parsed.offsetXYZ == std::array<float,3>{0, 0, 0};
        }
        catch (const std::exception&) {}
        check(matched, (std::string("SPR fixed float input preserves every output field: ") + item.text).c_str());
    }
    for (const char* invalid : {"+-1", "++1", "0x-1", "1x", "nan", "inf", "1e100", "1e-9999"})
    {
        const auto prior = priorSprite();
        assets::SprFile published = prior;
        bool rejected = false;
        try
        {
            std::istringstream input(std::string("sprite.mat 0 ") + invalid + " 2 4 3 3 0 0 0 0");
            published = assets::loadFromSPR(input);
        }
        catch (const std::runtime_error& error)
        {
            rejected = std::string(error.what()) == "Bad numeric data in JKDF2/MotS SPR.";
        }
        check(rejected && exactSprite(published, prior),
            (std::string("SPR malformed/nonfinite/unrepresentable fixed token preserves caller result: ") + invalid).c_str());
    }
#ifdef _WIN32
    // MSVCRT provides this locale independently of installed POSIX locale
    // archives. Restore process numeric conventions before leaving the test.
    const char* previousLocale = std::setlocale(LC_NUMERIC, nullptr);
    struct RestoreNumericLocale
    {
        std::string name;
        ~RestoreNumericLocale() { (void)std::setlocale(LC_NUMERIC, name.c_str()); }
    } restore{previousLocale ? previousLocale : "C"};
    const bool commaLocale = std::setlocale(LC_NUMERIC, "German_Germany.1252") != nullptr
        && std::string(std::localeconv()->decimal_point) == ",";
    check(commaLocale, "SPR locale fixture installs a comma-decimal C numeric locale");
    if (commaLocale)
    {
        bool valid = false;
        try
        {
            std::istringstream input("sprite.mat 0 1.25 2.5 4 3 3 0.5 1.125 2.25 3.5");
            const auto parsed = assets::loadFromSPR(input);
            valid = parsed.width == 1.25f && parsed.height == 2.5f
                && parsed.extraLightRGBA == std::array<float,4>{0.5f, 0.5f, 0.5f, 1.0f}
                && parsed.offsetXYZ == std::array<float,3>{1.125f, 2.25f, 3.5f};
        }
        catch (const std::exception&) {}
        check(valid, "SPR fixed dot-decimal fields parse identically under comma process locale");
        bool rejectedComma = false;
        try
        {
            std::istringstream input("sprite.mat 0 1,25 2 4 3 3 0 0 0 0");
            (void)assets::loadFromSPR(input);
        }
        catch (const std::runtime_error& error)
        {
            rejectedComma = std::string(error.what()) == "Bad numeric data in JKDF2/MotS SPR.";
        }
        check(rejectedComma, "SPR rejects comma numeric tokens even under comma process locale");
    }
#endif
    std::istringstream matInput("sprite.mat\nignored.mat");
    check(assets::matFromSPR(matInput) == "sprite.mat", "SPR material-only reader returns exactly the first complete token");
}
}
