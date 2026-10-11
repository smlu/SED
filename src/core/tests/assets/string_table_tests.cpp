#include "test_common.hpp"

#include "sed/assets/string_table.hpp"

#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sed::tests
{

void runStringTableTests()
{
    {
        const std::string rawByte(1U, static_cast<char>(0xE9));
        const std::string source =
            "MSGS 4\n"
            "# full-line comment with \"quote\"\n"
            "\"zKey\" -17 \"first C:\\path\\file\"\n"
            "\"CaseKey\" 0 \"literal # mark and \\\"quoted\\\" text\"\n"
            "\"casekey\" 214 \"" + rawByte + "\"\n"
            "\nEND\n";

        std::istringstream input(source);
        const sed::assets::StringTableFile strings = sed::assets::loadFromStringTableUni(
            input, sed::assets::UniProfile::ijim, "jonesstrings.uni");
        check(strings.capacity == 4U && strings.entries.size() == 3U,
            "UNI: retains the declared capacity and ordered records");
        if ( strings.entries.size() == 3U )
        {
            check(strings.entries[0].key == "zKey"
                    && strings.entries[0].numericValue == -17
                    && strings.entries[0].value == "first C:\\path\\file",
                "UNI: preserves key spelling, signed number, source order, and literal backslashes");
            check(strings.entries[1].key == "CaseKey"
                    && strings.entries[1].numericValue == 0
                    && strings.entries[1].value == "literal # mark and \\\"quoted\\\" text",
                "UNI: IJIM preserves embedded quotes and hash characters inside message text");
            check(strings.entries[2].key == "casekey"
                    && strings.entries[2].numericValue == 214
                    && strings.entries[2].value == rawByte,
                "UNI: IJIM preserves distinct key case and raw game bytes");
        }

        const std::string saved = sed::assets::saveToStringTableUniText(
            strings, sed::assets::UniProfile::ijim);
        const std::string expected =
            "MSGS 3\n\n"
            "\"zKey\" -17 \"first C:\\path\\file\"\n"
            "\"CaseKey\" 0 \"literal # mark and \\\"quoted\\\" text\"\n"
            "\"casekey\" 214 \"" + rawByte + "\"\n\nEND\n";
        check(saved == expected,
            "UNI: IJIM fixed writer vector is exact and keeps authored row order");
        std::istringstream roundTrip(saved);
        const sed::assets::StringTableFile loaded = sed::assets::loadFromStringTableUni(
            roundTrip, sed::assets::UniProfile::ijim, "round-trip.uni");
        check(loaded.capacity == strings.entries.size()
                && loaded.entries.size() == strings.entries.size()
                && loaded.entries[0].key == "zKey"
                && loaded.entries[1].key == "CaseKey"
                && loaded.entries[2].key == "casekey"
                && loaded.entries[1].value == strings.entries[1].value
                && loaded.entries[2].value == rawByte,
            "UNI: IJIM writer round-trips authored order, key case, quotes, and game bytes");
        check(saved.starts_with("MSGS 3\n"),
            "UNI: IJIM serialization canonicalizes overdeclared source capacity to the live row count");

        sed::assets::StringTableFile grown{};
        grown.entries.push_back({ "NewKey", 0, "new value" });
        check(sed::assets::saveToStringTableUniText(
                  grown, sed::assets::UniProfile::ijim)
                == "MSGS 1\n\n\"NewKey\" 0 \"new value\"\n\nEND\n",
            "UNI: IJIM writer declares the live row count after adding records beyond source capacity");
    }

    {
        const std::string source =
            "MSGS 1\n"
            "\"KEY\" 7 \"say \"yes#no\" now\"\n"
            "END\n";
        std::istringstream input(source);
        const sed::assets::StringTableFile strings = sed::assets::loadFromStringTableUni(
            input, sed::assets::UniProfile::ijim, "jonesstrings.uni");
        check(strings.entries.size() == 1U
                && strings.entries[0].value == "say \"yes#no\" now",
            "UNI: IJIM preserves bare embedded quote bytes and inline hash text inside the outer quoted field");
        check(sed::assets::saveToStringTableUniText(
                  strings, sed::assets::UniProfile::ijim)
                == "MSGS 1\n\n\"KEY\" 7 \"say \"yes#no\" now\"\n\nEND\n",
            "UNI: IJIM writer preserves retail embedded quote spelling without backslash escaping");
    }

    {
        bool exactWhitespaceKeyPreserved = false;
        std::string actualError{};
        try
        {
            std::istringstream input("MSGS 1\n\" KEY \" 0 \"value\"\nEND\n");
            const sed::assets::StringTableFile strings = sed::assets::loadFromStringTableUni(
                input, sed::assets::UniProfile::ijim, "jonesstrings.uni");
            exactWhitespaceKeyPreserved = strings.entries.size() == 1U
                && strings.entries[0].key == " KEY "
                && sed::assets::saveToStringTableUniText(
                       strings, sed::assets::UniProfile::ijim)
                    == "MSGS 1\n\n\" KEY \" 0 \"value\"\n\nEND\n";
        }
        catch ( const std::exception& exception )
        {
            actualError = exception.what();
        }
        const std::string whitespaceKeyCheck = actualError.empty()
            ? "UNI: IJIM preserves exact key whitespace and spelling"
            : "UNI: IJIM preserves exact key whitespace and spelling (actual error: "
                + actualError + ")";
        check(exactWhitespaceKeyPreserved, whitespaceKeyCheck.c_str());
    }

    {
        std::istringstream input("MSGS 1\n\"Plus\" +214 \"number\"\nEND\n");
        const sed::assets::StringTableFile strings = sed::assets::loadFromStringTableUni(
            input, sed::assets::UniProfile::classic, "cogstrings.uni");
        check(strings.entries.size() == 1U
                && strings.entries[0].numericValue == 214
                && sed::assets::saveToStringTableUniText(
                       strings, sed::assets::UniProfile::classic)
                    == "MSGS 1\n\n\"Plus\" 214 \"number\"\n\nEND\n",
            "UNI: classic accepts strtol-style leading plus and writes a normalized signed decimal field");
    }

    {
        std::istringstream maximumInput("MSGS 4096\nEND\n");
        const sed::assets::StringTableFile maximum = sed::assets::loadFromStringTableUni(
            maximumInput, sed::assets::UniProfile::ijim, "maximum-capacity.uni");
        check(maximum.capacity == 4096U,
            "UNI: admits the exact bounded MSGS capacity limit");

        checkThrowsWithMessage(
            []()
            {
                std::istringstream input("MSGS 4097\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "too-large.uni");
            },
            "too-large.uni parse error at line 1: MSGS capacity exceeds the 4096-row editor limit",
            "UNI: rejects capacities above the bounded editor admission limit");
    }

    {
        std::istringstream maximumLine(
            "MSGS 0\n#" + std::string(4095U, 'x') + "\nEND\n");
        const sed::assets::StringTableFile accepted = sed::assets::loadFromStringTableUni(
            maximumLine, sed::assets::UniProfile::ijim, "maximum-line.uni");
        check(accepted.entries.empty(),
            "UNI: accepts a physical line at the 4096-byte editor limit");
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input(
                    "MSGS 0\n#" + std::string(4096U, 'x') + "\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "overlong-line.uni");
            },
            "overlong-line.uni parse error at line 2: line exceeds the 4096-byte editor limit",
            "UNI: rejects a physical line above the 4096-byte editor limit");
    }

    {
        const auto row = [](const std::size_t valueBytes)
        {
            std::string text = "\"ABCDEFGHIJK\" -2147483648 \"";
            text.append(valueBytes, 'x');
            text += "\"\nEND\n";
            return text;
        };
        bool maximumAccepted = true;
        bool tooLongRejected = false;
        try
        {
            std::istringstream input("MSGS 1\n" + row(245U));
            (void)sed::assets::loadFromStringTableUni(
                input, sed::assets::UniProfile::ijim, "max-row.uni");
        }
        catch ( const std::runtime_error& )
        {
            maximumAccepted = false;
        }
        try
        {
            std::istringstream input("MSGS 1\n" + row(246U));
            (void)sed::assets::loadFromStringTableUni(
                input, sed::assets::UniProfile::ijim, "long-row.uni");
        }
        catch ( const std::runtime_error& )
        {
            tooLongRejected = true;
        }
        check(maximumAccepted && tooLongRejected,
            "UNI: IJIM row admission matches its bounded fixed-line game buffer");
    }

    {
        const std::string base = "\"KEY\" 0 \"value\"";
        const auto paddedRecord = [&base](const std::size_t targetBytes, const std::string_view eol)
        {
            std::string record = base;
            if ( record.size() < targetBytes )
            {
                record.append(targetBytes - record.size(), ' ');
            }
            return std::string("MSGS 1\n") + record + std::string(eol) + "END\n";
        };
        checkThrowsWithMessage(
            [paddedRecord]()
            {
                std::istringstream input(paddedRecord(274U, "\n"));
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "padded-lf.uni");
            },
            "padded-lf.uni parse error at line 2: message record exceeds the engine line limit",
            "UNI: IJIM bounds physical LF record bytes before trimming trailing whitespace");
        checkThrowsWithMessage(
            [paddedRecord]()
            {
                std::istringstream input(paddedRecord(274U, "\r\n"));
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "padded-crlf.uni");
            },
            "padded-crlf.uni parse error at line 2: message record exceeds the engine line limit",
            "UNI: IJIM applies the same physical record bound to CRLF input");
    }

    {
        const auto row = [](const std::size_t valueBytes)
        {
            std::string text = "\"ABCDEFGHIJK\" -2147483648 \"";
            text.append(valueBytes, 'x');
            text += "\"\nEND\n";
            return text;
        };
        bool maximumAccepted = true;
        bool tooLongRejected = false;
        try
        {
            std::istringstream input("MSGS 1\n" + row(225U));
            (void)sed::assets::loadFromStringTableUni(
                input, sed::assets::UniProfile::classic, "classic-max-row.uni");
        }
        catch ( const std::runtime_error& )
        {
            maximumAccepted = false;
        }
        try
        {
            std::istringstream input("MSGS 1\n" + row(226U));
            (void)sed::assets::loadFromStringTableUni(
                input, sed::assets::UniProfile::classic, "classic-long-row.uni");
        }
        catch ( const std::runtime_error& )
        {
            tooLongRejected = true;
        }
        check(maximumAccepted && tooLongRejected,
            "UNI: classic row admission matches its fixed-line game buffer");
    }

    {
        bool distinctCasePreserved = false;
        try
        {
            std::istringstream input(
                "MSGS 2\n\"CaseKey\" 0 \"one\"\n\"casekey\" 0 \"two\"\nEND\n");
            const sed::assets::StringTableFile strings = sed::assets::loadFromStringTableUni(
                input, sed::assets::UniProfile::classic, "cogstrings.uni");
            distinctCasePreserved = strings.entries.size() == 2U
                && strings.entries[0].key == "CaseKey"
                && strings.entries[1].key == "casekey"
                && sed::assets::saveToStringTableUniText(strings, sed::assets::UniProfile::classic)
                    == "MSGS 2\n\n\"CaseKey\" 0 \"one\"\n\"casekey\" 0 \"two\"\n\nEND\n";
        }
        catch ( const std::runtime_error& )
        {
        }
        check(distinctCasePreserved,
            "UNI: generic classic key identity preserves distinct byte case exactly like the engine hash table");
    }

    {
        const std::array<sed::assets::UniProfile, 2> profiles{
            sed::assets::UniProfile::classic, sed::assets::UniProfile::ijim };
        for ( const sed::assets::UniProfile profile : profiles )
        {
            std::istringstream input("MSGS 5\n\"ONE\" -17 \"one\"\n\"TWO\" 214 \"two\"\nEND\n");
            const sed::assets::StringTableFile strings = sed::assets::loadFromStringTableUni(
                input, profile, "source-capacity.uni");
            check(strings.capacity == 5U && strings.entries.size() == 2U
                    && sed::assets::saveToStringTableUniText(strings, profile)
                        == "MSGS 2\n\n\"ONE\" -17 \"one\"\n\"TWO\" 214 \"two\"\n\nEND\n"
                    && strings.capacity == 5U,
                "UNI: both engine writers emit the exact live count without changing retained source capacity");
            sed::assets::StringTableFile reduced = strings;
            reduced.entries.pop_back();
            check(sed::assets::saveToStringTableUniText(reduced, profile)
                    == "MSGS 1\n\n\"ONE\" -17 \"one\"\n\nEND\n",
                "UNI: removing a row updates the external engine count for both profiles");
            checkThrowsWithMessage(
                [profile]()
                {
                    std::istringstream duplicate("MSGS 2\n\"SameKey\" 0 \"one\"\n\"SameKey\" 0 \"two\"\nEND\n");
                    (void)sed::assets::loadFromStringTableUni(duplicate, profile, "exact-duplicate.uni");
                },
                "exact-duplicate.uni parse error at line 3: duplicate message key SameKey",
                "UNI: both engine profiles reject exact duplicate byte keys with the source and row");
        }
    }

    {
        checkThrowsWithMessage(
            []()
            {
                sed::assets::StringTableFile strings{};
                strings.capacity = 1U;
                strings.entries.push_back({ "STRICT", 0, "value with \" quote" });
                (void)sed::assets::saveToStringTableUniText(
                    strings, sed::assets::UniProfile::classic);
            },
            "UNI writer: classic string-table text cannot contain a double quote",
            "UNI: classic writer retains the existing strict embedded-quote contract");
    }

    {
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input(
                    "MSGS 1\n\"KEY\" 0 \"text\" trailing\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "actual-name.uni");
            },
            "actual-name.uni parse error at line 2: message text must be the final quoted field",
            "UNI: rejects post-value content with exact source and line diagnostics");
    }

    {
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input(
                    "MSGS 2\n\"Key\" 0 \"one\"\n\"Key\" 1 \"two\"\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "duplicate.uni");
            },
            "duplicate.uni parse error at line 3: duplicate message key Key",
            "UNI: duplicate diagnostics include exact source, row, and spelling");
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input(
                    "MSGS 1\n\"KEY\" 0 \"value\"\n\"EXTRA\" 0 \"row\"\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "over-capacity.uni");
            },
            "over-capacity.uni parse error at line 3: message records exceed declared MSGS capacity",
            "UNI: reader rejects message rows beyond the declared capacity");
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input("MSGS 1\n\"KEY 0 value\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "missing-key-quote.uni");
            },
            "missing-key-quote.uni parse error at line 2: message key must be quoted",
            "UNI: malformed key quoting has a stable source diagnostic");
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input("MSGS 1\n\"KEY\" 0 value\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::classic, "missing-value-quote.uni");
            },
            "missing-value-quote.uni parse error at line 2: message text must be the final quoted field",
            "UNI: malformed message-value quoting has a stable source diagnostic");
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input("MSGS 0\nEND\nAFTER\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "after-end.uni");
            },
            "after-end.uni parse error at line 3: unexpected content after END",
            "UNI: rejects authored content after END");
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input("MSGS 0\nEND\nDOS\x1a" "EOF\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "dos-eof.uni");
            },
            "dos-eof.uni parse error at line 3: embedded NUL/DOS EOF is not supported",
            "UNI: rejects DOS EOF markers");
    }

    {
        std::string invalid = "MSGS 1\n\"KEY\" 0 \"text\"";
        invalid.push_back('\0');
        checkThrowsWithMessage(
            [invalid]()
            {
                std::istringstream input(invalid);
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "eof-nul.uni");
            },
            "eof-nul.uni parse error at line 2: embedded NUL/DOS EOF is not supported",
            "UNI: rejects NUL bytes even on a final line without a newline");
    }

    {
        const auto unknownProfile = static_cast<sed::assets::UniProfile>(255U);
        checkThrowsWithMessage(
            [unknownProfile]()
            {
                std::istringstream input("MSGS 0\nEND\n");
                (void)sed::assets::loadFromStringTableUni(input, unknownProfile, "unknown.uni");
            },
            "Unknown UNI profile.",
            "UNI: rejects unknown parser profiles");
        checkThrowsWithMessage(
            [unknownProfile]()
            {
                (void)sed::assets::saveToStringTableUniText({}, unknownProfile);
            },
            "Unknown UNI profile.",
            "UNI: rejects unknown writer profiles");
    }

    {
        checkThrowsWithMessage(
            []()
            {
                sed::assets::StringTableFile strings{};
                strings.capacity = 1U;
                strings.entries.push_back({ "ABCDEFGHIJK", -2147483648, std::string(246U, 'x') });
                (void)sed::assets::saveToStringTableUniText(
                    strings, sed::assets::UniProfile::ijim);
            },
            "UNI writer: message record exceeds the engine line limit",
            "UNI: writer applies the IJIM line-size boundary");
    }

    {
        checkThrowsWithMessage(
            []()
            {
                sed::assets::StringTableFile strings{};
                strings.capacity = 2U;
                strings.entries = { { "SameKey", 0, "one" }, { "SameKey", 0, "two" } };
                (void)sed::assets::saveToStringTableUniText(
                    strings, sed::assets::UniProfile::classic);
            },
            "UNI writer: duplicate message key SameKey",
            "UNI: generic classic writer rejects exact duplicate byte keys");
        checkThrowsWithMessage(
            []()
            {
                sed::assets::StringTableFile strings{};
                strings.capacity = 2U;
                strings.entries = { { "SameKey", 0, "one" }, { "SameKey", 0, "two" } };
                (void)sed::assets::saveToStringTableUniText(
                    strings, sed::assets::UniProfile::ijim);
            },
            "UNI writer: duplicate message key SameKey",
            "UNI: IJIM writer rejects exact duplicate keys");
    }

    {
        sed::assets::StringTableFile grown{};
        grown.capacity = 1U;
        grown.entries = { { "ONE", 0, "1" }, { "TWO", 0, "2" } };
        bool grownClassicWritesLiveCount = false;
        try
        {
            grownClassicWritesLiveCount = sed::assets::saveToStringTableUniText(
                grown, sed::assets::UniProfile::classic)
                == "MSGS 2\n\n\"ONE\" 0 \"1\"\n\"TWO\" 0 \"2\"\n\nEND\n";
        }
        catch ( const std::runtime_error& )
        {
        }
        check(grownClassicWritesLiveCount && grown.capacity == 1U,
            "UNI: classic external writer grows the live count without mutating retained source capacity");
        checkThrowsWithMessage(
            []()
            {
                sed::assets::StringTableFile strings{};
                strings.capacity = 4097U;
                (void)sed::assets::saveToStringTableUniText(
                    strings, sed::assets::UniProfile::ijim);
            },
            "UNI writer: MSGS capacity exceeds the 4096-row editor limit",
            "UNI: writer rejects unbounded source capacity");
        checkThrowsWithMessage(
            []()
            {
                sed::assets::StringTableFile strings{};
                strings.capacity = 1U;
                strings.entries.push_back({ "KEY", 0, "line\nbreak" });
                (void)sed::assets::saveToStringTableUniText(
                    strings, sed::assets::UniProfile::ijim);
            },
            "UNI writer: string-table records cannot contain a quote in the key or an embedded line break/NUL",
            "UNI: writer rejects embedded line breaks in raw game-byte text");
        checkThrowsWithMessage(
            []()
            {
                sed::assets::StringTableFile strings{};
                strings.capacity = 1U;
                strings.entries.push_back({ "KEY", 0, std::string("nul\0byte", 8U) });
                (void)sed::assets::saveToStringTableUniText(
                    strings, sed::assets::UniProfile::ijim);
            },
            "UNI writer: string-table records cannot contain a quote in the key or an embedded line break/NUL",
            "UNI: writer rejects embedded NUL game bytes");
    }

    {
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input("MSGS invalid\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::classic, "bad-header.uni");
            },
            "bad-header.uni parse error at line 1: invalid MSGS capacity",
            "UNI: reports malformed capacity headers with their source and line");
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input("MSGS 1\n\"KEY\" +2147483648 \"text\"\nEND\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "bad-number.uni");
            },
            "bad-number.uni parse error at line 2: invalid signed decimal numeric field",
            "UNI: rejects signed numeric overflow");
        checkThrowsWithMessage(
            []()
            {
                std::istringstream input("MSGS 0\n");
                (void)sed::assets::loadFromStringTableUni(
                    input, sed::assets::UniProfile::ijim, "missing-end.uni");
            },
            "missing-end.uni parse error at line 1: missing END after message list",
            "UNI: rejects a missing END marker with the actual source name");
    }
}

} // namespace sed::tests
