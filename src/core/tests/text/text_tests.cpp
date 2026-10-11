#include "test_common.hpp"

#include <limits>
#include <optional>

#include "sed/parse.hpp"
#include "sed/text.hpp"

namespace sed::tests
{

void runTextTests()
{
    using namespace std::string_view_literals;
    struct TrimVector
    {
        std::string_view input, left, right, both;
    };
    for (const auto& v : std::initializer_list<TrimVector>{
             {"", "", "", ""},
             {" \t\r\n\v\f", "", "", ""},
             {"  a b \t", "a b \t", "  a b", "a b"},
             {"abc", "abc", "abc", "abc"},
             {" \0x "sv, "\0x "sv, " \0x"sv, "\0x"sv}})
    {
        check(text::trimLeft(v.input) == v.left, "trimLeft fixed vector");
        check(text::trimRight(v.input) == v.right, "trimRight fixed vector");
        check(text::trimView(v.input) == v.both, "trimView fixed vector");
        check(text::trim(v.input) == v.both, "trim owned fixed vector");
    }

    struct CommentVector
    {
        std::string_view input, hash, all;
    };
    for (const auto& v : std::initializer_list<CommentVector>{
             {"", "", ""}, {"abc", "abc", "abc"},
             {"#abc", "", ""}, {"a #b; c//d", "a ", "a "},
             {"a;b#c", "a;b", "a"}, {"a//b#c", "a//b", "a"},
             {"a/b", "a/b", "a/b"},
             {"a\0b#c"sv, "a\0b"sv, "a\0b"sv}})
    {
        std::string hash(v.input), all(v.input);
        text::stripHashComment(hash);
        text::stripLineComments(all);
        check(hash == v.hash, "stripHashComment complete inout fixed vector");
        check(all == v.all, "stripLineComments complete inout fixed vector");
    }

    struct CaseVector
    {
        std::string_view input, upper, lower;
    };
    for (const auto& v : std::initializer_list<CaseVector>{
             {"", "", ""}, {"aBc09._-", "ABC09._-", "abc09._-"},
             {"a\0B"sv, "A\0B"sv, "a\0b"sv}})
    {
        check(text::upperAscii(v.input) == v.upper, "upperAscii fixed vector");
        check(text::lowerAscii(v.input) == v.lower, "lowerAscii fixed vector");
        check(text::iequalsAscii(v.input, v.upper), "iequalsAscii complete true vector");
    }
    check(!text::iequalsAscii("abc", "abd"), "iequalsAscii different content");
    check(!text::iequalsAscii("ab", "abc"), "iequalsAscii different size");
    struct MatchVector
    {
        std::string_view input, pattern;
        bool starts, ends;
    };
    for (const auto& v : std::initializer_list<MatchVector>{
             {"", "", true, true}, {"abc", "", true, true},
             {"ab", "abc", false, false}, {"AbC", "aB", true, false},
             {"AbC", "Bc", false, true}, {"abc", "d", false, false}})
    {
        check(text::istartsWithAscii(v.input, v.pattern) == v.starts,
            "istartsWithAscii fixed return vector");
        check(text::iendsWithAscii(v.input, v.pattern) == v.ends,
            "iendsWithAscii fixed return vector");
    }
    check(text::splitWords("").empty(), "splitWords empty");
    check(text::splitWords(" \t\n\v\f\r").empty(), "splitWords all whitespace");
    check(text::splitWords("  a\tb\nc\vd\fe\rf ")
            == std::vector<std::string_view>{"a", "b", "c", "d", "e", "f"},
        "splitWords complete fixed vector");
    check(text::splitWords("a\0b c"sv) == std::vector<std::string_view>{"a\0b"sv, "c"},
        "splitWords embedded null remains a word byte");

    struct IntVector
    {
        std::string_view input;
        int base;
        std::optional<std::int32_t> expected;
    };
    for (const auto& v : std::initializer_list<IntVector>{
             {"0", 10, 0}, {"-2147483648", 10, INT32_MIN},
             {"2147483647", 10, INT32_MAX}, {"101", 2, 5},
             {"-ff", 16, -255}, {"z", 36, 35}, {"", 10, {}},
             {"2147483648", 10, {}}, {"-2147483649", 10, {}},
             {"+1", 10, {}}, {" 1", 10, {}}, {"1 ", 10, {}},
             {"1x", 10, {}}, {"2", 2, {}}, {"0x10", 16, {}},
             {"1\0"sv, 10, {}}})
    {
        check(text::parseInt<std::int32_t>(v.input, v.base) == v.expected,
            "parseInt presence and value fixed vector");
    }
    check(text::parseInt<std::uint32_t>("4294967295") == UINT32_MAX,
        "parseInt unsigned maximum");
    check(!text::parseInt<std::uint32_t>("4294967296"), "parseInt unsigned overflow");
    check(!text::parseInt<std::uint32_t>("-1"), "parseInt unsigned negative");
    struct HexVector
    {
        std::string_view input;
        std::optional<std::uint32_t> expected;
    };
    for (const auto& v : std::initializer_list<HexVector>{
             {"0", 0u}, {"0x10", 16u}, {"0XfF", 255u}, {"ffffffff", UINT32_MAX},
             {"100000000", {}}, {"0x", {}}, {"", {}}, {"-1", {}},
             {"0x1g", {}}, {" ff", {}}})
    {
        check(text::parseHex(v.input) == v.expected, "parseHex presence and value fixed vector");
    }
    struct DoubleVector
    {
        std::string_view input;
        std::optional<double> expected;
    };
    for (const auto& v : std::initializer_list<DoubleVector>{
             {"0", 0.0}, {"-1.25", -1.25}, {"+2.5e2", 250.0}, {" 1", 1.0},
             {"", {}}, {"1 ", {}}, {"1.2x", {}}, {"1,25", {}},
             {"1e9999", {}}, {"nan", {}}, {"inf", {}}, {"1\0"sv, {}}})
    {
        check(text::parseDouble(v.input) == v.expected,
            "parseDouble complete-token presence and value fixed vector");
    }

    struct RefVector
    {
        std::string_view input;
        std::int32_t sector, surface;
    };
    for (const auto& v : std::initializer_list<RefVector>{
             {"12:34", 12, 34}, {" 7 , 9 ", 7, 9}, {"(1;2)", 1, 2},
             {"-2147483648:2147483647", INT32_MIN, INT32_MAX}})
    {
        const auto result = parse::surfaceRef(v.input);
        check(result && result->sector == v.sector && result->surface == v.surface,
            "surfaceRef presence and both returned fields fixed vector");
    }
    for (const auto input : {"", "none", "NoNe", "-1", "1", "x:2", "1:x", "2147483648:0"})
    {
        check(!parse::surfaceRef(input), "surfaceRef invalid fixed vector");
    }
    check(format::surfaceRef({12, 34}) == "12:34", "surfaceRef formatter independent fixed vector");
    check(format::surfaceRef({INT32_MIN, INT32_MAX}) == "-2147483648:2147483647",
        "surfaceRef formatter signed endpoint fixed vector");
}

} // namespace sed::tests
