#include "test_common.hpp"

#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/parse_frame.hpp"

#include <chrono>
#include <bit>
#include <cmath>
#include <limits>
#include <streambuf>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace sed::tests
{

void runTplTests()
{
    {
        // Fixed decimal vectors are independent of both Value's parser and
        // formatter. Round-trip coverage below supplements these exact bytes.
        struct NumberVector { double value; const char* text; };
        const NumberVector vectors[] = {
            {0.0, "0"}, {-0.0, "-0"}, {0.1, "0.1"},
            {std::nextafter(1.0, 2.0), "1.0000000000000002"},
            {1e-8, "1e-08"},
            {std::numeric_limits<double>::min(), "2.2250738585072014e-308"},
            {std::numeric_limits<double>::denorm_min(), "5e-324"},
            {std::numeric_limits<double>::max(), "1.7976931348623157e+308"},
        };
        for (const auto& vector : vectors)
        {
            Value value;
            value.type = ValueType::floating;
            value.floatValue = vector.value;
            check(value.asString() == vector.text, "Value floating formatter matches complete fixed decimal vector");
            Value parsed;
            parsed.type = ValueType::floating;
            parsed.floatValue = 77.0;
            check(parsed.parse(vector.text)
                    && std::bit_cast<std::uint64_t>(parsed.floatValue) == std::bit_cast<std::uint64_t>(vector.value),
                "Value floating parser independently consumes finite boundary fixed vector");
        }
        check(formatTplTuple({-0.0, 1e-8, std::nextafter(1.0, 2.0)})
                == "(-0/1e-08/1.0000000000000002)",
            "Authored tuple formatter matches complete mixed-component fixed vector");
        TplValue frame;
        frame.type = ValueType::frame;
        frame.setFrame(-0.0, 1e-8, std::nextafter(1.0, 2.0), 4.5, -6.25, 90.0);
        check(frame.asString() == "(-0/1e-08/1.0000000000000002:4.5/-6.25/90)",
            "Frame setter matches complete independent six-component fixed vector");
        for (const char* input : {"", "(1/2)", "(1/2/3:4/5/bad)", "(1e309/2/3)",
                 "(1/2/3:4)", "(1/2/3:4/5)", "(1/2/3:4/5/6/7)", "(1/2/3:4/5/6) garbage"})
        {
            frame.stringValue = input;
            double x = 11, y = 22, z = 33, pch = 44, yaw = 55, rol = 66;
            check(!frame.tryGetFrame(x, y, z, pch, yaw, rol)
                    && x == 11 && y == 22 && z == 33 && pch == 44 && yaw == 55 && rol == 66,
                "Frame try-get rejection preserves every caller output");
            frame.getFrame(x, y, z, pch, yaw, rol);
            check(x == 0 && y == 0 && z == 0 && pch == 0 && yaw == 0 && rol == 0,
                "Frame defaulting get resets every output on invalid input");
            if (*input != '\0')
            {
                frame.stringValue = "(9/8/7:6/5/4)";
                check(!frame.parse(input) && frame.asString() == "(9/8/7:6/5/4)",
                    "Frame parse rejection preserves complete prior token without dropping partial/extra components");
            }
        }
        frame.stringValue = "(1/2/3)";
        double x = 11, y = 22, z = 33, pch = 44, yaw = 55, rol = 66;
        check(frame.tryGetFrame(x, y, z, pch, yaw, rol)
                && x == 1 && y == 2 && z == 3 && pch == 0 && yaw == 0 && rol == 0,
            "Frame three-component success publishes all six outputs including angle defaults");
    }
    {
        assets::Templates templates;
        ThingTemplate parent;
        parent.name = "  MixedCase_Parent  ";
        parent.parent = "none";
        parent.values.push_back(deserializeTplValue("mass", "17.25"));
        check(templates.addOrUpdate(parent) == 0, "Templates first insertion returns fixed index zero");
        const auto* found = templates.find("\t mixedcase_PARENT \r\n");
        check(found == templates.get(0) && found && found->name == "  MixedCase_Parent  "
                && !templates.find("missing") && !templates.find(" \t ") && !templates.get(1),
            "Templates lookup trims and folds ASCII while retaining authored name and exact missing results");
        ThingTemplate duplicate = parent;
        duplicate.name = "mixedcase_parent";
        duplicate.desc = "ignored duplicate";
        check(templates.addOrUpdate(duplicate, false) == 0 && templates.size() == 1
                && templates.get(0)->desc.empty(),
            "Templates update-disabled duplicate returns existing index without replacing data");
        ThingTemplate child;
        child.name = "Child";
        child.parent = " MIXEDCASE_parent ";
        check(templates.addOrUpdate(child) == 1, "Templates child insertion returns fixed index one");
        const auto* inherited = templates.findField(" child ", "MASS");
        check(inherited && inherited->asString() == "17.25"
                && templates.findField("missing", "mass") == nullptr
                && templates.findField("child", "absent") == nullptr,
            "Templates inheritance matches mixed-case fixed value and complete missing outcomes");
        duplicate.desc = "replacement";
        check(templates.addOrUpdate(duplicate, true) == 0 && templates.size() == 2
                && templates.get(0)->name == "mixedcase_parent" && templates.get(0)->desc == "replacement",
            "Templates updating duplicate preserves index and publishes exact replacement fields");
        parent.name = "cycleA"; parent.parent = "cycleB"; parent.values.clear();
        child.name = "cycleB"; child.parent = "cycleA";
        templates.addOrUpdate(parent); templates.addOrUpdate(child);
        check(!templates.findField("cycleA", "absent"), "Templates bounded cyclic inheritance returns no field");
        child.name = "orphan"; child.parent = "missing";
        templates.addOrUpdate(child);
        check(!templates.findField("orphan", "absent"), "Templates missing parent returns no field");
        templates.clear();
        check(templates.size() == 0 && !templates.get(0) && !templates.find("child")
                && !templates.findField("mixedcase_parent", "mass"),
            "Templates clear empties both record and lookup state");
    }
    {
        // A device failure after a complete record must not publish a partial
        // database. EOF, including a final line without newline, remains valid.
        class FailingInputBuffer final : public std::streambuf
        {
        public:
            FailingInputBuffer()
            {
                setg(prefix_.data(), prefix_.data(), prefix_.data() + prefix_.size());
            }
        private:
            int_type underflow() override { throw std::runtime_error("injected template device failure"); }
            std::string prefix_{"first none mass=1\nsecond none mass="};
        } buffer;
        std::istream failedInput(&buffer);
        assets::Templates published;
        ThingTemplate original;
        original.name = "existing";
        original.parent = "none";
        published.addOrUpdate(original);
        bool rejected = false;
        try { published = assets::loadTemplates(failedInput); }
        catch (const std::runtime_error& error)
        {
            rejected = true;
            check(std::string(error.what()) == "Failed to read templates.",
                "TPL stream read failure reports complete fixed diagnostic");
        }
        check(rejected && published.size() == 1 && published.find("existing")
                && !published.find("first") && !published.find("second"),
            "TPL stream device failure rejects partial publication and preserves caller database");
        std::istringstream finalLine("last none mass=2");
        const auto complete = assets::loadTemplates(finalLine);
        const auto* mass = complete.findField("last", "mass");
        check(complete.size() == 1 && complete.find("last")
                && mass && mass->asString() == "2",
            "TPL ordinary EOF admits a complete final line without newline");
    }
    {
        const auto sameBits = [](double a, double b)
        {
            return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
        };
        for (const double expected : {std::nextafter(1.0, 2.0), 123456789.12345679,
                 1.234567890123456e-120, -0.0, 0.1})
        {
            sed::TplValue value;
            value.name = "mass";
            value.type = sed::ValueType::floating;
            value.floatValue = expected;
            sed::Value parsed;
            parsed.type = sed::ValueType::floating;
            check(parsed.parse(value.asString()) && sameBits(parsed.floatValue, expected),
                "Value floating text roundtrips every authored bit");
            const auto tuple = sed::parse::frameValue(sed::formatTplTuple({expected, expected, expected}));
            check(tuple && sameBits(tuple->x, expected) && sameBits(tuple->y, expected)
                    && sameBits(tuple->z, expected),
                "Authored template tuples roundtrip each exact component");
            sed::ThingTemplate tpl;
            tpl.name = "precise_parameter";
            tpl.parent = "none";
            tpl.values.push_back(value);
            sed::assets::Templates templates;
            templates.addOrUpdate(tpl, true);
            std::ostringstream stream;
            sed::assets::saveTemplates(stream, templates);
            std::istringstream input(stream.str());
            const auto restored = sed::assets::loadTemplates(input);
            const auto* actual = restored.findField(tpl.name, "mass");
            // TPL has no type tags: generic scalar parameters load as text
            // until template definitions supply their editor type.
            sed::Value restoredNumber;
            restoredNumber.type = sed::ValueType::floating;
            check(actual && restoredNumber.parse(actual->asString())
                    && sameBits(restoredNumber.floatValue, expected),
                "TPL floating parameter save and load preserve exact authored precision");
        }
    }
    {
        sed::ThingTemplate tpl;
        tpl.name = "precise_template";
        tpl.parent = "none";
        tpl.bbox.Set(1.0e-8, -0.0, -1.0e-11,
            std::nextafter(1.0, 2.0), 123456789.1250001, 9.876543210987654);
        sed::assets::Templates templates;
        templates.addOrUpdate(tpl, true);
        std::ostringstream stream;
        sed::assets::saveTemplates(stream, templates);
        std::istringstream input(stream.str());
        const auto reloaded = sed::assets::loadTemplates(input);
        const auto* actual = reloaded.find(tpl.name);
        const auto sameBits = [](double a, double b)
        {
            return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
        };
        check(actual && sameBits(actual->bbox.x1, tpl.bbox.x1)
                && sameBits(actual->bbox.y1, tpl.bbox.y1)
                && sameBits(actual->bbox.z1, tpl.bbox.z1)
                && sameBits(actual->bbox.x2, tpl.bbox.x2)
                && sameBits(actual->bbox.y2, tpl.bbox.y2)
                && sameBits(actual->bbox.z2, tpl.bbox.z2),
            "TPL save preserves exact finite BBOX coordinates, including sub-micro values and signed zero");

        class RejectingBuffer final : public std::streambuf
        {
            std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
            int_type overflow(int_type = traits_type::eof()) override { return traits_type::eof(); }
        } buffer;
        std::ostream failingStream(&buffer);
        bool rejected = false;
        try { sed::assets::saveTemplates(failingStream, templates); }
        catch ( const std::exception& ) { rejected = true; }
        check(rejected, "TPL stream save reports device-write failure instead of silently succeeding");

        const auto directory = std::filesystem::temp_directory_path()
            / ("sed-tpl-save-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
        struct Cleanup
        {
            std::filesystem::path path;
            ~Cleanup()
            {
                std::error_code error;
                std::filesystem::remove_all(path, error);
            }
        } cleanup{directory};
        const auto destination = directory / "templates.tpl";
        const std::string previous = "previous complete template database\n";
        {
            std::ofstream output(destination, std::ios::binary);
            output << previous;
        }
        const auto readFile = [](const std::filesystem::path& path)
        {
            std::ifstream inputFile(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(inputFile), {});
        };
        tpl.bbox.x1 = std::numeric_limits<double>::quiet_NaN();
        sed::assets::Templates invalid;
        invalid.addOrUpdate(tpl, true);
        rejected = false;
        try { sed::assets::saveTemplates(destination, invalid); }
        catch ( const std::exception& ) { rejected = true; }
        check(rejected && readFile(destination) == previous,
            "TPL path save rejects non-finite BBOX without damaging an existing database");
        sed::assets::saveTemplates(destination, templates);
        check(readFile(destination) == stream.str(),
            "TPL path save atomically publishes the exact checked stream representation");
        std::filesystem::remove(destination);
        std::filesystem::create_directory(destination);
        const auto protectedFile = destination / "old.tpl";
        {
            std::ofstream output(protectedFile, std::ios::binary);
            output << previous;
        }
        rejected = false;
        try { sed::assets::saveTemplates(destination, templates); }
        catch ( const std::exception& ) { rejected = true; }
        check(rejected && readFile(protectedFile) == previous
                && std::distance(std::filesystem::directory_iterator(directory),
                    std::filesystem::directory_iterator{}) == 1,
            "TPL publication failure preserves existing contents and cleans staging files");
    }
    {
        struct TupleCase
        {
            const char* name;
            const char* input;
            const char* expected;
        };
        const TupleCase tupleCases[] = {
            { "light", "1/0.5/2e-1/4", "(1/0.5/2e-1/4)" },
            { "light", "(1/0.5/2e-1/4)", "(1/0.5/2e-1/4)" },
            { "light", ".1/.2/.3", "(.1/.2/.3)" },
            { "lightintensity", "-1/0/2.5", "(-1/0/2.5)" },
            { "lightintensity", "0.2/0.4/0.8/6", "(0.2/0.4/0.8/6)" },
            { "lightoffset", "0 / .07 / .04", "(0/.07/.04)" },
            { "eyeoffset", "1,2,3", "(1/2/3)" },
            { "fireoffset", "1;2;3", "(1/2/3)" },
            { "orient", "90/0/-45", "(90/0/-45)" },
            { "vel", "0/4/0", "(0/4/0)" },
            { "angvel", "5/10/0", "(5/10/0)" },
            { "flashrgb", "80/90/80", "(80/90/80)" },
            { "flashrgb", "+80/-90/0", "(+80/-90/0)" },
            { "spritestart", ".01/.01/1", "(.01/.01/1)" },
            { "spriteend", ".17/.17/0", "(.17/.17/0)" },
            { "frame", "1/2/3:4/5/6", "(1/2/3:4/5/6)" },
            { "frame", "1 2 3", "(1/2/3)" },
            { "voicecolor", "1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16",
                "(1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16)" },
        };

        for (const TupleCase& test : tupleCases)
        {
            const sed::TplValue value = sed::deserializeTplValue(test.name, test.input);
            check(value.asString() == test.expected, "TPL.Value.deserialize canonical tuple");
            check(sed::serializeTplValue(value) == test.expected, "TPL.Value.serialize canonical tuple");
        }

        const sed::TplValue classicLight = sed::deserializeTplValue("light", ".8");
        const sed::TplValue classicHeadlight = sed::deserializeTplValue("lightintensity", "1");
        const sed::TplValue unrelatedPath = sed::deserializeTplValue("material", "folder/1/2/test.mat");
        const sed::TplValue invalidArity = sed::deserializeTplValue("lightoffset", "1/2");
        const sed::TplValue malformedTuple = sed::deserializeTplValue("light", "1//2/3/4");
        const sed::TplValue fractionalFlash = sed::deserializeTplValue("flashrgb", "1.5/2/3");
        const sed::TplValue exponentFlash = sed::deserializeTplValue("flashrgb", "1e2/2/3");
        const sed::TplValue overflowFlash = sed::deserializeTplValue("flashrgb", "2147483648/2/3");
        check(sed::serializeTplValue(classicLight) == ".8"
                && sed::serializeTplValue(classicHeadlight) == "1",
            "TPL.Value keeps classic scalar light parameters scalar");
        check(sed::serializeTplValue(unrelatedPath) == "folder/1/2/test.mat",
            "TPL.Value leaves unrelated slash strings untouched");
        check(sed::serializeTplValue(invalidArity) == "1/2"
                && sed::serializeTplValue(malformedTuple) == "1//2/3/4",
            "TPL.Value does not repair malformed tuple parameters silently");
        check(sed::serializeTplValue(fractionalFlash) == "1.5/2/3"
                && sed::serializeTplValue(exponentFlash) == "1e2/2/3"
                && sed::serializeTplValue(overflowFlash) == "2147483648/2/3",
            "TPL.Value does not canonicalize non-integer flashrgb components");
        check(sed::formatTplTuple({ -1.25, 0.0, 2.5 }) == "(-1.25/0/2.5)",
            "TPL.Value tuple formatter emits one parenthesized token");

        const sed::TplValue ijimRgbHeadlight =
            sed::deserializeIjimNdyTplValue("lightintensity", "(0.2/0.4/0.8)");
        const sed::TplValue ijimRangedHeadlight =
            sed::deserializeIjimNdyTplValue("lightintensity", "0.2/0.4/0.8/6");
        const sed::TplValue ijimRegularLight =
            sed::deserializeIjimNdyTplValue("light", "0.1/0.2/0.3/4");
        const sed::TplValue classicScalarHeadlight =
            sed::deserializeIjimNdyTplValue("lightintensity", "1");
        check(ijimRgbHeadlight.asString() == "(0.2/0.4/0.8/1)",
            "TPL.Value IJIM NDY load materializes RGB Headlight editor range");
        check(ijimRangedHeadlight.asString() == "(0.2/0.4/0.8/6)"
                && sed::serializeIjimNdyTplValue(ijimRangedHeadlight) == "(0.2/0.4/0.8)",
            "TPL.Value IJIM NDY writer strips unsupported Headlight range");
        check(sed::serializeIjimNdyTplValue(ijimRegularLight) == "(0.1/0.2/0.3/4)"
                && sed::serializeIjimNdyTplValue(classicScalarHeadlight) == "1",
            "TPL.Value IJIM NDY compatibility leaves regular light and scalar values unchanged");

        sed::TplValue arbitraryVector{};
        arbitraryVector.name = "customvector";
        arbitraryVector.type = sed::ValueType::vect;
        arbitraryVector.stringValue = "1 / 2 / 3 / 4 / 5";
        sed::TplValue malformedIjimHeadlight{};
        malformedIjimHeadlight.name = "lightintensity";
        malformedIjimHeadlight.type = sed::ValueType::str;
        malformedIjimHeadlight.stringValue = "(bad/0.4/0.8/6)";
        check(sed::serializeIjimNdyTplValue(arbitraryVector) == "(1/2/3/4/5)",
            "TPL.Value IJIM NDY compatibility preserves arbitrary vectors");
        check(sed::serializeIjimNdyTplValue(malformedIjimHeadlight) == "(bad/0.4/0.8/6)",
            "TPL.Value IJIM NDY compatibility preserves malformed Headlight text");

        const sed::TplValue damageClass = sed::deserializeTplValue("damageclass", "0x8000000");
        check(damageClass.type == sed::ValueType::flag
                && damageClass.intValue == 0x8000000
                && sed::serializeTplValue(damageClass) == "0x8000000",
            "TPL.Value damageclass uses canonical hexadecimal flag serialization");

        constexpr std::string_view flagNames[] = {
            "thingflags", "typeflags", "physflags", "damageclass"
        };
        for (const std::string_view flagName : flagNames)
        {
            const sed::TplValue bare = sed::deserializeTplValue(flagName, "10");
            const sed::TplValue prefixed = sed::deserializeTplValue(flagName, "0X10");
            const sed::TplValue highBit = sed::deserializeTplValue(flagName, "80000000");
            const sed::TplValue negative = sed::deserializeTplValue(flagName, "-10");
            check(bare.type == sed::ValueType::flag && bare.intValue == 0x10
                    && sed::serializeTplValue(bare) == "0x10"
                    && prefixed.intValue == 0x10
                    && sed::serializeTplValue(prefixed) == "0x10",
                "TPL.Value flag parameters parse bare and prefixed hexadecimal values");
            check(static_cast<std::uint32_t>(highBit.intValue) == 0x80000000u
                    && sed::serializeTplValue(highBit) == "0x80000000"
                    && static_cast<std::uint32_t>(negative.intValue) == 0xfffffff0u
                    && sed::serializeTplValue(negative) == "0xfffffff0",
                "TPL.Value flag parameters preserve high-bit and signed scanf-hex semantics");
        }
    }

    {
        const char* tpl =
            R"TPL(
                # DESC: Example template
                # BBOX: 1 2 3 4 5 6
                temp1 none material=test.mat thingflags=0x10 typeflags=0x8110009 physflags=0x4a4 orient=(0.000000/0.000000/0.000000)
                child temp1 thingflags=0x20
            )TPL";

        std::istringstream iss(tpl);
        const sed::assets::Templates tpls = sed::assets::loadTemplates(iss);

        check(tpls.size() == 2, "TPL: count");

        const sed::assets::ThingTemplate* base = tpls.find("temp1");
        check(base != nullptr, "TPL: find base");
        check(base && base->desc == "Example template", "TPL: desc");
        check(base && base->bbox.x1 == 1.0 && base->bbox.y1 == 2.0 && base->bbox.z1 == 3.0, "TPL: bbox min");
        check(base && base->bbox.x2 == 4.0 && base->bbox.y2 == 5.0 && base->bbox.z2 == 6.0, "TPL: bbox max");

        const sed::TplValue* material = tpls.findField("temp1", "material");
        check(material != nullptr && material->adjType == sed::AdjType::mat, "TPL: material adjType");

        const sed::TplValue* flags = tpls.findField("temp1", "thingflags");
        check(flags != nullptr && flags->type == sed::ValueType::flag && flags->intValue == 0x10, "TPL: thingflags parse");
        const sed::TplValue* typeFlags = tpls.findField("temp1", "typeflags");
        const sed::TplValue* physFlags = tpls.findField("temp1", "physflags");
        check(typeFlags != nullptr && typeFlags->type == sed::ValueType::flag
                && typeFlags->intValue == 0x8110009,
            "TPL: typeflags parse");
        check(physFlags != nullptr && physFlags->type == sed::ValueType::flag
                && physFlags->intValue == 0x4a4,
            "TPL: physflags parse");

        const sed::TplValue* orient = tpls.findField("temp1", "orient");
        check(orient != nullptr && orient->type == sed::ValueType::str && orient->stringValue == "(0.000000/0.000000/0.000000)",
              "TPL: orient string");

        const sed::TplValue* inheritedMat = tpls.findField("child", "material");
        check(inheritedMat != nullptr && inheritedMat->adjType == sed::AdjType::mat, "TPL: inherit field");
    }

    {
        const char* tpl = R"TPL(
            duplicate_base none light=0.25 LIGHT=0.75
            duplicate_child duplicate_base
            duplicate_override duplicate_base light=1 light=2
        )TPL";
        std::istringstream input(tpl);
        const sed::assets::Templates templates = sed::assets::loadTemplates(input);

        const sed::TplValue* baseLight = templates.findField("duplicate_base", "light");
        check(baseLight != nullptr && baseLight->asString() == "0.75",
            "TPL: duplicate scalar lookup is case-insensitive and last-wins");

        const sed::TplValue* inheritedLight = templates.findField("duplicate_child", "light");
        check(inheritedLight != nullptr && inheritedLight->asString() == "0.75",
            "TPL: inherited duplicate scalar lookup keeps base last-wins value");

        const sed::TplValue* overriddenLight = templates.findField("duplicate_override", "light");
        check(overriddenLight != nullptr && overriddenLight->asString() == "2",
            "TPL: nearest template duplicate scalar lookup is last-wins");
    }

    {
        const char* tpl = R"TPL(
            tuple_base none light=1/0.5/0.25/8 lightintensity=(0.2/0.4/0.8/6) lightoffset=0/.07/.04 eyeoffset=(0/.01/.0735) vel=0/4/0 angvel=(5/10/0) fireoffset=.01/.02/.03 orient=90/0/-45 spritestart=.01/.01/1 spriteend=.17/.17/0 flashrgb=80/90/80 voicecolor=1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16 frame=1/2/3:4/5/6 frame=(7/8/9) material=folder/1/2/test.mat damageclass=0x20
            classic tuple_base light=.8 lightintensity=1
        )TPL";
        std::istringstream input(tpl);
        const sed::assets::Templates loaded = sed::assets::loadTemplates(input);

        std::ostringstream savedStream;
        sed::assets::saveTemplates(savedStream, loaded);
        const std::string savedText = savedStream.str();
        const std::string expectedText =
            "# DESC: \n# BBOX: 0 0 0 0 0 0\n"
            "tuple_base none light=(1/0.5/0.25/8) lightintensity=(0.2/0.4/0.8/6) lightoffset=(0/.07/.04) "
            "eyeoffset=(0/.01/.0735) vel=(0/4/0) angvel=(5/10/0) fireoffset=(.01/.02/.03) orient=(90/0/-45) "
            "spritestart=(.01/.01/1) spriteend=(.17/.17/0) flashrgb=(80/90/80) "
            "voicecolor=(1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16) frame=(1/2/3:4/5/6) frame=(7/8/9) "
            "material=folder/1/2/test.mat damageclass=0x20\n"
            "# DESC: \n# BBOX: 0 0 0 0 0 0\n"
            "classic tuple_base light=.8 lightintensity=1\n";
        check(savedText == expectedText,
            "TPL writer matches complete fixed database including metadata, order, repeated fields and whitespace");

        std::istringstream reparsedInput(savedText);
        const sed::assets::Templates reparsed = sed::assets::loadTemplates(reparsedInput);
        const sed::TplValue* reparsedLight = reparsed.findField("tuple_base", "light");
        const sed::TplValue* reparsedDamage = reparsed.findField("tuple_base", "damageclass");
        check(reparsedLight && reparsedLight->asString() == "(1/0.5/0.25/8)"
                && reparsedDamage && reparsedDamage->type == sed::ValueType::flag
                && reparsedDamage->intValue == 0x20,
            "TPL.Roundtrip reparses canonical tuple and typed flag values");
    }

    {
        const auto unique                      = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root       = "tmp_tpl_root_" + unique;
        const std::filesystem::path file       = root / "misc" / "master.tpl";

        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        check(!ec, "TPL.ResourceLocator.create_directories");

        {
            std::ofstream out(file);
            out << "# DESC: Example template\n";
            out << "temp1 none thingflags=0x10\n";
        }

        try
        {
            sed::assets::ResourceLocator locator{};
            locator.addRoot(root);

            const sed::assets::Templates tpls = sed::assets::loadTemplates(locator, "master.tpl");
            const sed::TplValue* flags = tpls.findField("temp1", "thingflags");
            check(flags != nullptr && flags->type == sed::ValueType::flag && flags->intValue == 0x10,
                  "TPL.ResourceLocator.loadTemplates");
        }
        catch (const std::exception& e)
        {
            check(false, e.what());
        }

        std::filesystem::remove_all(root, ec);
    }

    {
        sed::assets::Templates templates{};

        sed::ThingTemplate base{};
        base.name = "base";
        base.parent = "none";
        base.desc = "Roundtrip template";
        base.bbox.Set(-1.0, -2.0, -3.0, 4.0, 5.0, 6.0);

        sed::TplValue material{};
        material.name = "material";
        material.type = sed::tplValueTypeForName(material.name);
        material.adjType = sed::tplAdjTypeForName(material.name);
        check(material.parse("test.mat"), "TPL.SaveRoundtrip.material.parse");
        base.values.push_back(material);

        sed::TplValue flags{};
        flags.name = "thingflags";
        flags.type = sed::tplValueTypeForName(flags.name);
        flags.adjType = sed::tplAdjTypeForName(flags.name);
        check(flags.parse("0x21"), "TPL.SaveRoundtrip.flags.parse");
        base.values.push_back(flags);

        templates.addOrUpdate(base, true);

        std::ostringstream oss;
        sed::assets::saveTemplates(oss, templates);

        check(oss.str() == "# DESC: Roundtrip template\n# BBOX: -1 -2 -3 4 5 6\nbase none material=test.mat thingflags=0x21\n",
            "TPL writer emits complete fixed metadata and parameter vector");
        std::istringstream iss(oss.str());
        const sed::assets::Templates reparsed = sed::assets::loadTemplates(iss);
        check(reparsed.size() == 1, "TPL.SaveRoundtrip.count");

        const sed::assets::ThingTemplate* saved = reparsed.find("base");
        check(saved != nullptr, "TPL.SaveRoundtrip.find");
        check(saved && saved->desc == "Roundtrip template", "TPL.SaveRoundtrip.desc");
        check(saved && saved->bbox.x1 == -1.0 && saved->bbox.y2 == 5.0, "TPL.SaveRoundtrip.bbox");

        const sed::TplValue* savedMaterial = reparsed.findField("base", "material");
        check(savedMaterial != nullptr && savedMaterial->stringValue == "test.mat", "TPL.SaveRoundtrip.material");

        const sed::TplValue* savedFlags = reparsed.findField("base", "thingflags");
        check(savedFlags != nullptr && savedFlags->intValue == 0x21, "TPL.SaveRoundtrip.flags");
    }
}

} // namespace sed::tests
