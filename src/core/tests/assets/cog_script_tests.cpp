
#include "test_common.hpp"

#include "sed/assets/cog_script.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace sed::tests
{

void runCogScriptTests()
{
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path root = "tmp_cog_script_root_" + unique;
    const std::filesystem::path file = root / "cog" / "test.cog";

    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    check(!ec, "CogScript.create_directories");

    {
        std::ofstream out(file, std::ios::binary);
        check(static_cast<bool>(out), "CogScript.write_file.open");
        out
            << "# comment\n"
            << "symbols\n"
            << "thing thg0=3 local\n"
            << "sector mySector\n"
            << "thing myThing=5\n"
            << "surface srf0\n"
            << "surface srfPair=1 2\n"
            << "thing myTarget\n"
            << "sector brokenSector=oops\n"
            << "flex myFloat=1.5\n"
            << "int myInt=7\n"
            << "int defaultInt\n"
            << "flex defaultFloat\n"
            << "template defaultTpl\n"
            << "mat defaultMat\n"
            << "sound defaultSound=door_open.wav\n"
            << "message startup local\n"
            << "end\n";
        check(static_cast<bool>(out), "CogScript.write_file.write");
    }

    sed::assets::ResourceLocator locator{};
    locator.addRoot(root);

    const auto noLocals = sed::assets::loadCogSymbolsNoLocals(locator, "test");
    check(noLocals.size() == 13, "CogScript.loadCogSymbolsNoLocals.size");

    const auto withDefaults = sed::assets::loadCogSymbolsWithDefaults(locator, "test", false);
    check(withDefaults.size() == 13, "CogScript.loadCogSymbolsWithDefaults.no_locals.size");

    auto findByName = [](const std::vector<sed::CogValue>& values, const std::string_view name) -> const sed::CogValue*
    {
        for (const sed::CogValue& v : values)
        {
            if (sed::text::iequalsAscii(v.name, name))
            {
                return &v;
            }
        }
        return nullptr;
    };

    auto findByNameOccurrence = [](const std::vector<sed::CogValue>& values,
        const std::string_view name,
        const std::size_t occurrence) -> const sed::CogValue*
    {
        std::size_t seen = 0;
        for (const sed::CogValue& v : values)
        {
            if (!sed::text::iequalsAscii(v.name, name))
            {
                continue;
            }
            if (seen++ == occurrence)
            {
                return &v;
            }
        }
        return nullptr;
    };

    {
        const sed::CogValue* v = findByName(withDefaults, "myFloat");
        check(v != nullptr && v->type == sed::ValueType::floating, "CogScript.defaults.myFloat.type");
        check(v != nullptr && close(v->floatValue, 1.5), "CogScript.defaults.myFloat.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "myInt");
        check(v != nullptr && v->type == sed::ValueType::integer, "CogScript.defaults.myInt.type");
        check(v != nullptr && v->intValue == 7, "CogScript.defaults.myInt.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "mySector");
        check(v != nullptr && v->type == sed::ValueType::ptr, "CogScript.defaults.mySector.type");
        check(v != nullptr && v->intValue == -1, "CogScript.defaults.mySector.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "srf0");
        check(v != nullptr && v->type == sed::ValueType::ptr, "CogScript.defaults.srf0.type");
        check(v != nullptr && v->intValue == -1, "CogScript.defaults.srf0.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "srfPair");
        check(v != nullptr && v->type == sed::ValueType::ptr, "CogScript.defaults.srfPair.type");
        check(v != nullptr && v->intValue == -1, "CogScript.defaults.srfPair.value");
        check(v != nullptr && v->stringValue == "1 2", "CogScript.defaults.srfPair.raw");

        sed::Level level{};
        sed::Sector& sector = level.newSector();
        sector.num = 1;
        sector.surfaces.emplace_back();
        sector.surfaces.back().num = 2;

        check(v != nullptr && v->asJedString(level) == "1 2", "CogScript.defaults.srfPair.asJedString");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "myTarget");
        check(v != nullptr && v->type == sed::ValueType::ptr, "CogScript.defaults.myTarget.type");
        check(v != nullptr && v->intValue == -1, "CogScript.defaults.myTarget.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "myThing");
        check(v != nullptr && v->type == sed::ValueType::ptr, "CogScript.defaults.myThing.type");
        check(v != nullptr && v->intValue == 5, "CogScript.defaults.myThing.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "brokenSector");
        check(v != nullptr && v->type == sed::ValueType::ptr, "CogScript.defaults.brokenSector.type");
        check(v != nullptr && v->intValue == -1, "CogScript.defaults.brokenSector.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "defaultInt");
        check(v != nullptr && v->type == sed::ValueType::integer, "CogScript.defaults.defaultInt.type");
        check(v != nullptr && v->intValue == 0, "CogScript.defaults.defaultInt.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "defaultFloat");
        check(v != nullptr && v->type == sed::ValueType::floating, "CogScript.defaults.defaultFloat.type");
        check(v != nullptr && close(v->floatValue, 0.0), "CogScript.defaults.defaultFloat.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "defaultTpl");
        check(v != nullptr && v->type == sed::ValueType::str, "CogScript.defaults.defaultTpl.type");
        check(v != nullptr && v->stringValue.empty(), "CogScript.defaults.defaultTpl.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "defaultMat");
        check(v != nullptr && v->type == sed::ValueType::str, "CogScript.defaults.defaultMat.type");
        check(v != nullptr && v->stringValue.empty(), "CogScript.defaults.defaultMat.value");
    }

    {
        const sed::CogValue* v = findByName(withDefaults, "defaultSound");
        check(v != nullptr && v->cogType == sed::CogType::wav, "CogScript.defaults.defaultSound.cogType");
        check(v != nullptr && v->type == sed::ValueType::str, "CogScript.defaults.defaultSound.type");
        check(v != nullptr && v->stringValue == "door_open.wav", "CogScript.defaults.defaultSound.value");
    }

    const std::filesystem::path grammarFile = root / "cog" / "grammar.cog";
    {
        std::ofstream out(grammarFile, std::ios::binary);
        check(static_cast<bool>(out), "CogScript.grammar.write_file.open");
        out
            << "symbols\n"
            << "thing quotedHash desc=\"sector#1\" local # trailing comment\n"
            << "thing quotedSlash desc=\"sector//1\" local // trailing comment\n"
            << "thing emptyDescription desc=\"\" local\n"
            << "thing quotedDescription desc=\"keep local mask=7\" mask = 7\n"
            << "int spacedInt = 7 desc=\"count\"\n"
            << "int spacedIntNoGap =7\n"
            << "int spacedIntAfter= 8\n"
            << "flex spacedFloat = -2.5\n"
            << "vector spacedVector = (1/2/3)\n"
            << "vector compactVector=(4/5/6)\n"
            << "vector localVector = (7 / 8 / 9) local\n"
            << "cog emptyCog\n"
            << "ai emptyAi\n"
            << "key emptyKey\n"
            << "mat emptyMat\n"
            << "model emptyModel\n"
            << "sound emptySound\n"
            << "sound quotedSound = \"local\"\n"
            << "template emptyTemplate\n"
            << "template quotedTemplate = \"local\"\n"
            << "int emptyInt\n"
            << "flex emptyFloat\n"
            << "vector emptyVector\n"
            << "message emptyMessage\n"
            << "sector emptySector\n"
            << "surface emptySurface\n"
            << "thing emptyThing\n"
            << "end\n";
        check(static_cast<bool>(out), "CogScript.grammar.write_file.write");
    }

    const auto grammarNoLocals = sed::assets::loadCogSymbolsNoLocals(locator, "grammar");
    const auto grammarWithDefaults = sed::assets::loadCogSymbolsWithDefaults(locator, "grammar", false);
    const auto grammarWithLocals = sed::assets::loadCogSymbolsWithDefaults(locator, "grammar", true);

    check(grammarNoLocals.size() == 22, "CogScript.grammar.no_locals.count");
    check(grammarWithDefaults.size() == 22, "CogScript.grammar.defaults.count");
    check(grammarWithLocals.size() == 27, "CogScript.grammar.include_locals.count");

    {
        const sed::CogValue* v = findByName(grammarNoLocals, "quotedHash");
        check(v == nullptr, "CogScript.grammar.hash_inside_quote.local_is_seen");
    }
    {
        const sed::CogValue* v = findByName(grammarNoLocals, "quotedSlash");
        check(v == nullptr, "CogScript.grammar.slash_inside_quote.local_is_seen");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "quotedDescription");
        check(v != nullptr && v->description == "keep local mask=7",
            "CogScript.grammar.quoted_description.fixed_vector");
        check(v != nullptr && !v->local, "CogScript.grammar.quoted_description.not_local");
        check(v != nullptr && v->mask == 7, "CogScript.grammar.mask_assignment");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "spacedInt");
        check(v != nullptr && v->intValue == 7, "CogScript.grammar.spaced_integer_assignment");
        check(v != nullptr && v->description == "count", "CogScript.grammar.description_assignment");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "spacedFloat");
        check(v != nullptr && v->type == sed::ValueType::floating && close(v->floatValue, -2.5),
            "CogScript.grammar.spaced_float_assignment");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "spacedIntNoGap");
        check(v != nullptr && v->intValue == 7, "CogScript.grammar.assignment_no_gap_after_equals");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "spacedIntAfter");
        check(v != nullptr && v->intValue == 8, "CogScript.grammar.assignment_no_gap_before_equals");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "spacedVector");
        const sed::CogValue* compact = findByName(grammarWithDefaults, "compactVector");
        check(v != nullptr && v->type == sed::ValueType::vect
                && v->stringValue == "(0/0/0)"
                && compact != nullptr && compact->type == sed::ValueType::vect
                && compact->stringValue == "(0/0/0)",
            "CogScript.grammar.vector_declaration_initializers_are_ignored");
    }
    {
        const sed::CogValue* local = findByName(grammarWithLocals, "localVector");
        check(findByName(grammarWithDefaults, "localVector") == nullptr
                && local != nullptr && local->local
                && local->type == sed::ValueType::vect
                && local->stringValue == "(0/0/0)",
            "CogScript.grammar.local_vector_declaration_initializer_is_ignored");
    }

    const auto checkDefaultString = [&](const std::string_view name, const char* label)
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, name);
        check(v != nullptr && v->type == sed::ValueType::str && v->stringValue.empty(), label);
    };
    checkDefaultString("emptyAi", "CogScript.defaults.ai.empty_string");
    checkDefaultString("emptyKey", "CogScript.defaults.key.empty_string");
    checkDefaultString("emptyMat", "CogScript.defaults.mat.empty_string");
    checkDefaultString("emptyModel", "CogScript.defaults.model.empty_string");
    checkDefaultString("emptySound", "CogScript.defaults.sound.empty_string");
    checkDefaultString("emptyTemplate", "CogScript.defaults.template.empty_string");

    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "quotedSound");
        check(v != nullptr && v->stringValue == "local", "CogScript.grammar.quoted_sound_value");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "quotedTemplate");
        check(v != nullptr && v->stringValue == "local", "CogScript.grammar.quoted_template_value");
    }

    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "emptyCog");
        check(v != nullptr && v->type == sed::ValueType::integer && v->intValue == -1,
            "CogScript.defaults.cog.unset_is_minus_one");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "emptyInt");
        check(v != nullptr && v->type == sed::ValueType::integer && v->intValue == 0,
            "CogScript.defaults.int.zero");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "emptyFloat");
        check(v != nullptr && v->type == sed::ValueType::floating && close(v->floatValue, 0.0),
            "CogScript.defaults.float.zero");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "emptyVector");
        check(v != nullptr && v->type == sed::ValueType::vect && v->stringValue == "(0/0/0)",
            "CogScript.defaults.vector.zero_vector");
    }
    {
        const sed::CogValue* v = findByName(grammarWithLocals, "emptyMessage");
        check(v != nullptr && v->type == sed::ValueType::str && v->stringValue.empty(),
            "CogScript.defaults.message.empty_without_invented_value");
        check(findByName(grammarWithDefaults, "emptyMessage") == nullptr,
            "CogScript.defaults.message.excluded_without_locals");
    }

    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "emptySector");
        check(v != nullptr && v->type == sed::ValueType::ptr && v->intValue == -1,
            "CogScript.defaults.sector.unset_is_minus_one");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "emptySurface");
        check(v != nullptr && v->type == sed::ValueType::ptr && v->intValue == -1,
            "CogScript.defaults.surface.unset_is_minus_one");
    }
    {
        const sed::CogValue* v = findByName(grammarWithDefaults, "emptyThing");
        check(v != nullptr && v->type == sed::ValueType::ptr && v->intValue == -1,
            "CogScript.defaults.thing.unset_is_minus_one");
    }
    {
        const sed::CogValue* v = findByName(grammarWithLocals, "emptyThing");
        check(v != nullptr && v->type == sed::ValueType::ptr && v->intValue == -1,
            "CogScript.defaults.include_locals.public_thing_stays_minus_one");
    }

    {
        const sed::CogValue* v = findByName(grammarWithLocals, "quotedHash");
        check(v != nullptr && v->local, "CogScript.grammar.include_locals.hash_local");
        check(v != nullptr && v->description == "sector#1", "CogScript.grammar.include_locals.hash_description");
    }
    {
        const sed::CogValue* v = findByName(grammarWithLocals, "quotedSlash");
        check(v != nullptr && v->local, "CogScript.grammar.include_locals.slash_local");
        check(v != nullptr && v->description == "sector//1", "CogScript.grammar.include_locals.slash_description");
    }
    {
        const sed::CogValue* v = findByName(grammarWithLocals, "emptyDescription");
        check(v != nullptr && v->local && v->description.empty(),
            "CogScript.grammar.empty_quoted_attribute_does_not_consume_local");
    }

    const auto writeScript = [&](const std::string_view name, const std::string_view contents)
    {
        std::ofstream out(root / "cog" / std::string(name), std::ios::binary);
        check(static_cast<bool>(out), "CogScript.status.write_file.open");
        out << contents;
        check(static_cast<bool>(out), "CogScript.status.write_file.write");
    };
    writeScript("empty.cog", "symbols\nend\n");
    writeScript("truncated.cog", "symbols\nint value\n");
    writeScript("invalid_value.cog",
        "symbols\nflex badFloat=nan\nvector badVector=banana\nsurface badSurface=\"1 nope\"\nend\n");
    writeScript("invalid_schema.cog",
        "symbols\ninte typoType\nint duplicate\nint DUPLICATE\nend\n");
    writeScript("invalid_mask.cog",
        "symbols\nint badMask mask=not-a-mask\nend\n");
    writeScript("valid_mask.cog",
        "symbols\nint decimalMask mask=7\nint hexMask mask=0x10\nint unsetMask mask=-1\nend\n");
    writeScript("vector_initializer_ignored.cog",
        "symbols\n"
        "vector malformedInitializer=not-a-vector\n"
        "vector quotedInitializer='1 2 3'\n"
        "end\n");
    writeScript("compatibility.cog",
        "symbols\n"
        "local\n"
        "symbols\n"
        "int decimalInt=3.0;\n"
        "int exponentInt=1e3\n"
        "flex decimalFloat=1.0; local\n"
        "int bareHex=0 mask=10\n"
        "int prefixedHex=0 mask=0x10\n"
        "int emptyMask=0 mask\n"
        "int emptyMaskEquals=0 mask=\n"
        "thing typoLink=3 nolink1 desc=the thing youre blowing up\n"
        "int typoDescriptor=4 desc_track_to_loop_back_to\n"
        "int typoEqual=5 lindid=1\n"
        "int duplicate=1\n"
        "int DUPLICATE=2\n"
        "message startup\n"
        "message startup\n"
        "vector spacedVector=(1 / 2 / 3)\n"
        "vector completeVector=(1/2/3) nomessage\n"
        "surface spacedPair=1 2 nolink1\n"
        "int apostropheDescription=7 desc=designer's helper # comment\n"
        "sound apostropheResource=door's.wav\n"
        "thing quotedHash desc=\"a#b\" local\n"
        "thing quotedSlash desc=\"a//b\" local\n"
        "end;\n");
    writeScript("overflow_int.cog",
        "symbols\nint tooBig=999999999999999999999999\nend\n");
    writeScript("unterminated_quote.cog",
        "symbols\nthing bad desc='unterminated\nend\n");
    writeScript("no_symbols.cog", "code\nend\n");

    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "empty", false);
        check(result.status == sed::assets::CogSymbolLoadStatus::loaded,
            "CogScript.status.empty.loaded");
        check(result.loaded(), "CogScript.status.empty.loaded_helper");
        check(result.symbols.empty(), "CogScript.status.empty.symbols");
        check(result.diagnostic.empty(), "CogScript.status.empty.diagnostic");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "missing", false);
        check(result.status == sed::assets::CogSymbolLoadStatus::missing,
            "CogScript.status.missing.status");
        check(result.symbols.empty(), "CogScript.status.missing.symbols");
        check(result.diagnostic == "COG script resource was not found.",
            "CogScript.status.missing.diagnostic");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "truncated", false);
        check(result.status == sed::assets::CogSymbolLoadStatus::malformed,
            "CogScript.status.truncated.status");
        check(result.symbols.empty(), "CogScript.status.truncated.symbols");
        check(result.diagnostic == "COG script has no complete symbols section.",
            "CogScript.status.truncated.diagnostic");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "no_symbols", false);
        check(result.status == sed::assets::CogSymbolLoadStatus::malformed,
            "CogScript.status.no_symbols.status");
        check(result.symbols.empty(), "CogScript.status.no_symbols.symbols");
        check(result.diagnostic == "COG script has no complete symbols section.",
            "CogScript.status.no_symbols.diagnostic");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "invalid_value", false);
        check(result.status == sed::assets::CogSymbolLoadStatus::malformed,
            "CogScript.status.invalid_value.status");
        check(result.symbols.size() == 3, "CogScript.status.invalid_value.symbols_preserved");
        const sed::CogValue* badFloat = findByName(result.symbols, "badFloat");
        const sed::CogValue* badVector = findByName(result.symbols, "badVector");
        const sed::CogValue* badSurface = findByName(result.symbols, "badSurface");
        check(badFloat != nullptr && badFloat->floatValue == 0.0,
            "CogScript.status.invalid_value.float_safety_default");
        check(badVector != nullptr && badVector->stringValue == "(0/0/0)",
            "CogScript.status.invalid_value.vector_safety_default");
        check(badSurface != nullptr && badSurface->intValue == -1
                && badSurface->stringValue == "1 nope",
            "CogScript.status.invalid_value.surface_safety_default");
        check(result.diagnostic == "COG symbols section contains an invalid value.",
            "CogScript.status.invalid_value.diagnostic");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "invalid_schema", false);
        check(result.status == sed::assets::CogSymbolLoadStatus::malformed,
            "CogScript.status.invalid_schema.status");
        check(result.symbols.size() == 3,
            "CogScript.status.invalid_schema.compatibility_payload");
        check(result.diagnostic
                == "COG symbols section contains an unsupported declaration.",
            "CogScript.status.invalid_schema.diagnostic");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "invalid_mask", false);
        check(result.status == sed::assets::CogSymbolLoadStatus::malformed,
            "CogScript.status.invalid_mask.status");
        check(result.symbols.size() == 1,
            "CogScript.status.invalid_mask.compatibility_payload");
        check(result.diagnostic == "COG symbols section contains an invalid value.",
            "CogScript.status.invalid_mask.diagnostic");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "valid_mask", false);
        const sed::CogValue* decimal = findByName(result.symbols, "decimalMask");
        const sed::CogValue* hex = findByName(result.symbols, "hexMask");
        const sed::CogValue* unset = findByName(result.symbols, "unsetMask");
        check(result.status == sed::assets::CogSymbolLoadStatus::loaded
                && decimal && decimal->mask == 7
                && hex && hex->mask == 0x10
                && unset && unset->mask == static_cast<std::int32_t>(0xFFFFu),
            "CogScript.status.valid_mask.fixed_vectors");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(
            locator, "vector_initializer_ignored", false);
        const sed::CogValue* value = findByName(
            result.symbols, "malformedInitializer");
        const sed::CogValue* quoted = findByName(
            result.symbols, "quotedInitializer");
        check(result.status == sed::assets::CogSymbolLoadStatus::loaded
                && value != nullptr
                && value->type == sed::ValueType::vect
                && value->stringValue == "(0/0/0)"
                && quoted != nullptr
                && quoted->type == sed::ValueType::vect
                && quoted->stringValue == "(0/0/0)"
                && result.diagnostic.empty(),
            "CogScript.compatibility.malformed_vector_initializer_is_ignored");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "compatibility", false);
        const sed::CogValue* decimalInt = findByName(result.symbols, "decimalInt");
        const sed::CogValue* exponentInt = findByName(result.symbols, "exponentInt");
        const sed::CogValue* bareHex = findByName(result.symbols, "bareHex");
        const sed::CogValue* prefixedHex = findByName(result.symbols, "prefixedHex");
        const sed::CogValue* emptyMask = findByName(result.symbols, "emptyMask");
        const sed::CogValue* emptyMaskEquals = findByName(result.symbols, "emptyMaskEquals");
        const sed::CogValue* typoLink = findByName(result.symbols, "typoLink");
        const sed::CogValue* typoDescriptor = findByName(result.symbols, "typoDescriptor");
        const sed::CogValue* typoEqual = findByName(result.symbols, "typoEqual");
        const sed::CogValue* duplicate = findByNameOccurrence(result.symbols, "duplicate", 0);
        const sed::CogValue* duplicateUpper = findByNameOccurrence(result.symbols, "duplicate", 1);
        const sed::CogValue* spacedVector = findByName(result.symbols, "spacedVector");
        const sed::CogValue* completeVector = findByName(result.symbols, "completeVector");
        const sed::CogValue* spacedPair = findByName(result.symbols, "spacedPair");
        const sed::CogValue* apostropheDescription = findByName(result.symbols, "apostropheDescription");
        const sed::CogValue* apostropheResource = findByName(result.symbols, "apostropheResource");
        check(result.status == sed::assets::CogSymbolLoadStatus::loaded
                && result.symbols.size() == 16,
            "CogScript.compatibility.unknown_metadata_is_ignored");
        check(decimalInt && decimalInt->intValue == 3
                && exponentInt && exponentInt->intValue == 1,
            "CogScript.compatibility.atoi_integer_prefix");
        check(bareHex && bareHex->mask == 0x10
                && prefixedHex && prefixedHex->mask == 0x10
                && emptyMask && emptyMask->mask == 0
                && emptyMaskEquals && emptyMaskEquals->mask == 0,
            "CogScript.compatibility.sscanf_hex_mask");
        check(typoLink && typoLink->intValue == 3
                && typoDescriptor && typoDescriptor->intValue == 4
                && typoEqual && typoEqual->intValue == 5,
            "CogScript.compatibility.typo_attributes_do_not_shift_values");
        check(duplicate && duplicateUpper
                && duplicate->name == "duplicate" && duplicate->intValue == 1
                && duplicateUpper->name == "DUPLICATE" && duplicateUpper->intValue == 2,
            "CogScript.compatibility.duplicate_public_order");
        check(spacedVector && spacedVector->stringValue == "(0/0/0)"
                && completeVector && completeVector->stringValue == "(0/0/0)"
                && spacedPair && spacedPair->stringValue == "1 2",
            "CogScript.compatibility.vector_declaration_initializers_are_ignored");
        check(apostropheDescription && apostropheDescription->description == "designer's"
                && apostropheResource && apostropheResource->stringValue == "door's.wav",
            "CogScript.compatibility.apostrophes_are_literal_inside_words");

        const auto withLocals = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "compatibility", true);
        const sed::CogValue* decimalFloat = findByName(withLocals.symbols, "decimalFloat");
        const sed::CogValue* quotedHash = findByName(withLocals.symbols, "quotedHash");
        const sed::CogValue* quotedSlash = findByName(withLocals.symbols, "quotedSlash");
        check(withLocals.status == sed::assets::CogSymbolLoadStatus::loaded
                && withLocals.symbols.size() == 21
                && decimalFloat && close(decimalFloat->floatValue, 1.0)
                && quotedHash && quotedHash->description == "a#b"
                && quotedSlash && quotedSlash->description == "a//b",
            "CogScript.compatibility.trailing_semicolon_and_quoted_comments");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "overflow_int", false);
        const sed::CogValue* tooBig = findByName(result.symbols, "tooBig");
        check(result.status == sed::assets::CogSymbolLoadStatus::malformed
                && tooBig && tooBig->intValue == 0,
            "CogScript.compatibility.integer_overflow_is_rejected_safely");
    }
    {
        const auto result = sed::assets::loadCogSymbolsWithDefaultsResult(locator, "unterminated_quote", false);
        check(result.status == sed::assets::CogSymbolLoadStatus::malformed
                && result.symbols.empty()
                && result.diagnostic == "COG script contains an unterminated quoted value.",
            "CogScript.compatibility.unterminated_quotes_remain_malformed");
    }

    {
        const auto captured = sed::assets::loadCogSymbolsFromTextResult(
            "symbols\n"
            "int captured=17\n"
            "sound authored=door.wav\n"
            "end\n",
            false);
        const sed::CogValue* intValue = findByName(captured.symbols, "captured");
        const sed::CogValue* soundValue = findByName(captured.symbols, "authored");
        check(captured.status == sed::assets::CogSymbolLoadStatus::loaded
                && intValue && intValue->intValue == 17
                && soundValue && soundValue->stringValue == "door.wav",
            "CogScript.status.captured_text.fixed_vector");
    }

    std::filesystem::remove_all(root, ec);
}

} // namespace sed::tests
