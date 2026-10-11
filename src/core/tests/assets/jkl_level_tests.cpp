#include "test_common.hpp"

#include "sed/assets/jkl.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/thing.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>

namespace sed::tests
{

namespace
{

class ThrowAtEndStreamBuffer final : public std::streambuf
{
public:
    explicit ThrowAtEndStreamBuffer(std::string input) : input_(std::move(input))
    {
    }

protected:
    int_type underflow() override
    {
        if ( position_ == input_.size() )
        {
            throw std::runtime_error("injected JKL input failure");
        }
        return traits_type::to_int_type(input_[position_]);
    }

    int_type uflow() override
    {
        const int_type value = underflow();
        ++position_;
        return value;
    }

private:
    std::string input_{};
    std::size_t position_{};
};

} // namespace

void runJklLevelTests()
{
    const auto makeSectorLightingFixture = [](
        const int version,
        const std::string_view ambient,
        const std::string_view extra,
        const std::string_view tint = {})
    {
        std::ostringstream out;
        out << "SECTION: HEADER\n"
            << "VERSION " << version << "\n"
            << "END\n"
            << "SECTION: GEORESOURCE\n"
            << "WORLD COLORMAPS 0\n"
            << "WORLD VERTICES 0\n"
            << "WORLD TEXTURE VERTICES 0\n"
            << "WORLD ADJOINS 0\n"
            << "WORLD SURFACES 0\n"
            << "END\n"
            << "SECTION: SECTORS\n"
            << "WORLD SECTORS 1\n"
            << "SECTOR 0\n"
            << "FLAGS 0\n"
            << "AMBIENT LIGHT " << ambient << '\n'
            << "EXTRA LIGHT " << extra << '\n';
        if ( !tint.empty() )
        {
            out << "TINT " << tint << '\n';
        }
        out << "SURFACES 0 0\n"
            << "END\n";
        return out.str();
    };

    const auto makeEmptySurfaceSpanFixture = [](
        const std::string_view base,
        const std::string_view count)
    {
        return std::string(R"JKL(
        SECTION: HEADER
        VERSION 1
        END
        SECTION: GEORESOURCE
        WORLD COLORMAPS 0
        WORLD VERTICES 0
        WORLD TEXTURE VERTICES 0
        WORLD ADJOINS 0
        WORLD SURFACES 0
        END
        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0
        AMBIENT LIGHT 0
        EXTRA LIGHT 0
        SURFACES )JKL")
            + std::string(base) + " " + std::string(count)
            + R"JKL(
        END
        )JKL";
    };

    const auto makeSectorSectionFixture = [](
        const int version,
        const std::string_view sectorRecords)
    {
        std::ostringstream out;
        out << "SECTION: HEADER\n"
            << "VERSION " << version << "\n"
            << "END\n"
            << "SECTION: GEORESOURCE\n";
        if (version == 1)
        {
            out << "WORLD COLORMAPS 0\n";
        }
        out << "WORLD VERTICES 0\n"
            << "WORLD TEXTURE VERTICES 0\n"
            << "WORLD ADJOINS 0\n"
            << "WORLD SURFACES 0\n"
            << "END\n"
            << "SECTION: SECTORS\n";
        out << sectorRecords;
        out << "END\n";
        return out.str();
    };

    const auto makeInvalidWorldVertexReferenceFixture = [](
        const int version,
        const std::int32_t worldVertexIndex)
    {
        std::ostringstream out;
        out << "SECTION: HEADER\n"
            << "VERSION " << version << "\n"
            << "END\n"
            << "SECTION: GEORESOURCE\n";
        if (version == 1)
        {
            out << "WORLD COLORMAPS 0\n";
        }
        out << "WORLD VERTICES 3\n"
            << "0: 0 0 0\n"
            << "1: 1 0 0\n"
            << "2: 0 1 0\n"
            << "WORLD TEXTURE VERTICES 1\n"
            << "0: 0 0\n"
            << "WORLD ADJOINS 0\n"
            << "WORLD SURFACES 1\n";
        if (version == 1)
        {
            out << "0: -1 0x0 0x0 0 3 3 -1 0 3 0,0 1,0 "
                << worldVertexIndex << ",0\n";
        }
        else
        {
            out << "0: -1 0x0 0x0 0 3 3 -1 0 0 0 1 3 0,0 1,0 "
                << worldVertexIndex << ",0\n";
        }
        out << "END\n"
            << "SECTION: SECTORS\n"
            << "WORLD SECTORS 1\n"
            << "SECTOR 0\n"
            << "FLAGS 0\n"
            << (version == 1 ? "AMBIENT LIGHT 0\nEXTRA LIGHT 0\n"
                             : "AMBIENT LIGHT 0 0 0\nEXTRA LIGHT 0 0 0\n")
            << "BOUNDBOX 0 0 0 0 0 0\n"
            << "CENTER 0 0 0\n"
            << "RADIUS 0\n"
            << "VERTICES 3\n"
            << "0: 0\n"
            << "1: 1\n"
            << "2: 2\n"
            << "SURFACES 0 1\n"
            << "END\n";
        return out.str();
    };

    const auto makeIjimColorFixture = [](
        const std::string_view fog = "11.1 11.2 11.3 11.4",
        const std::string_view ambient = "21.1 21.2 21.3",
        const std::string_view extra = "22.1 22.2 22.3",
        const std::string_view average = "23.1 23.2 23.3",
        const std::string_view tint = "24.1 24.2 24.3",
        const std::string_view surfaceExtra = "25.1 25.2 25.3 25.4",
        const std::string_view corners = "26.1 26.2 26.3 26.4 26.5 26.6 26.7 26.8 26.9",
        const std::string_view editorLight = "27.1 27.2 27.3")
    {
        std::ostringstream out;
        out << "SECTION: HEADER\n"
            << "VERSION 3\n"
            << "FOG 1 " << fog << " 0 250\n"
            << "END\n"
            << "SECTION: GEORESOURCE\n"
            << "WORLD VERTICES 3\n"
            << "0: 0 0 0\n"
            << "1: 1 0 0\n"
            << "2: 0 1 0\n"
            << "WORLD TEXTURE VERTICES 3\n"
            << "0: 0 0\n"
            << "1: 1 0\n"
            << "2: 0 1\n"
            << "WORLD ADJOINS 0\n"
            << "WORLD SURFACES 1\n"
            << "0: -1 0x0 0x0 0 3 3 -1 " << surfaceExtra
            << " 3 0,0 1,1 2,2 " << corners << "\n"
            << "END\n"
            << "SECTION: SECTORS\n"
            << "WORLD SECTORS 1\n"
            << "SECTOR 0\n"
            << "FLAGS 0\n"
            << "AMBIENT LIGHT " << ambient << '\n'
            << "EXTRA LIGHT " << extra << '\n'
            << "AVERAGE LIGHT INTENSITY " << average << '\n'
            << "TINT " << tint << '\n'
            << "SURFACES 0 1\n"
            << "END\n"
            << "SECTION: LIGHTS\n"
            << "EDITOR LIGHTS 1\n"
            << "0: 0x0 0 0 0 0 2 10 " << editorLight << '\n'
            << "END\n";
        return out.str();
    };

    const auto makeVersion1SurfaceColorFixture = [](
        const std::string_view corners)
    {
        return std::string(R"JKL(
        SECTION: HEADER
        VERSION 1
        END
        SECTION: GEORESOURCE
        WORLD COLORMAPS 0
        WORLD VERTICES 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        WORLD TEXTURE VERTICES 3
        0: 0 0
        1: 1 0
        2: 0 1
        WORLD ADJOINS 0
        WORLD SURFACES 1
        0: -1 0x0 0x0 0 3 3 -1 0 3 0,0 1,1 2,2 )JKL")
            + std::string(corners)
            + R"JKL(
        END
        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0
        AMBIENT LIGHT 0
        EXTRA LIGHT 0
        SURFACES 0 1
        END
        )JKL";
    };

    {
        std::string validWithoutFinalLf = makeSectorLightingFixture(1, "0.25", "-0.5");
        if ( !validWithoutFinalLf.empty() && validWithoutFinalLf.back() == '\n' )
        {
            validWithoutFinalLf.pop_back();
        }
        std::istringstream input(validWithoutFinalLf);
        const sed::Level level = sed::assets::loadFromJKL(input);
        check(level.sectors.size() == 1
                && close(level.sectors[0].ambient.r, 0.25)
                && close(level.sectors[0].extraLight.r, -0.5),
            "JKL input stream: valid EOF without a final LF publishes the complete level");
    }

    {
        const std::string source = makeSectorLightingFixture(1, "0.25", "-0.5");
        ThrowAtEndStreamBuffer buffer(source);
        std::istream input(&buffer);

        sed::Level published{};
        published.clear();
        published.header.gravity = 73.25;
        sed::Sector& sentinelSector = published.newSector();
        sentinelSector.flags = 0x1234;
        sentinelSector.ambient = sed::ColorF{ -2.0f, 4.0f, 8.0f, 0.5f };
        const sed::Level before = published;

        std::string diagnostic;
        try
        {
            published = sed::assets::loadFromJKL(input);
        }
        catch (const std::exception& exception)
        {
            diagnostic = exception.what();
        }
        check(diagnostic
                == "Cannot load JKL/NDY: input stream failed while reading level text.",
            "JKL input stream: device failure at EOF has a fixed diagnostic");
        check(input.bad() && sed::assets::sedLevelsEqual(published, before),
            "JKL input stream: a throwing streambuf cannot publish partial caller state");
    }

    {
        struct ProjectFixture
        {
            sed::ProjectType project;
            int version;
        };
        constexpr std::array<ProjectFixture, 3> projects{
            ProjectFixture{ sed::ProjectType::JKDF2, 1 },
            ProjectFixture{ sed::ProjectType::MOTS, 1 },
            ProjectFixture{ sed::ProjectType::IJIM, 3 },
        };
        for ( const ProjectFixture& fixture : projects )
        {
            std::string oneEmptySectorRecords =
                "WORLD SECTORS 1\n"
                "SECTOR 0\n"
                "FLAGS 0\n";
            if ( fixture.version == 1 )
            {
                oneEmptySectorRecords +=
                    "AMBIENT LIGHT 0\n"
                    "EXTRA LIGHT 0\n";
            }
            else
            {
                oneEmptySectorRecords +=
                    "AMBIENT LIGHT 0 0 0\n"
                    "EXTRA LIGHT 0 0 0\n";
            }
            oneEmptySectorRecords +=
                "BOUNDBOX 0 0 0 0 0 0\n"
                "CENTER 0 0 0\n"
                "RADIUS 0\n"
                "VERTICES 0\n";
            oneEmptySectorRecords += "SURFACES 0 0\n";
            const std::string oneEmptySector = makeSectorSectionFixture(
                fixture.version, oneEmptySectorRecords);
            sed::assets::JklLoadOptions options{};
            options.projectType = fixture.project;
            std::istringstream input(oneEmptySector);
            const sed::Level level = sed::assets::loadFromJKL(input, options);
            check(level.kind == fixture.project
                    && level.sectors.size() == 1
                    && level.sectors[0].vertices.empty()
                    && level.sectors[0].surfaces.empty(),
                "JKL.SectorRecords: each supported project keeps a complete legal empty sector");

            const std::string noSectors = makeSectorSectionFixture(
                fixture.version,
                "WORLD SECTORS 0\n");
            std::istringstream emptyInput(noSectors);
            const sed::Level empty = sed::assets::loadFromJKL(emptyInput, options);
            check(empty.kind == fixture.project && empty.sectors.empty(),
                "JKL.SectorRecords: a zero-sector world remains valid for each supported project");

            const std::string invalidReference =
                makeInvalidWorldVertexReferenceFixture(fixture.version, 3);
            constexpr std::string_view worldVertexDiagnostic =
                "Cannot load JKL/NDY surface: world vertex index is invalid.";
            checkThrowsWithMessage(
                [&invalidReference, &options]()
                {
                    std::istringstream malformedInput(invalidReference);
                    (void)sed::assets::loadFromJKL(malformedInput, options);
                },
                worldVertexDiagnostic,
                "JKL.SectorRecords: an out-of-range world vertex reference is rejected for each supported project");

            const std::string validReference =
                makeInvalidWorldVertexReferenceFixture(fixture.version, 2);
            std::istringstream validInput(validReference);
            const sed::Level valid =
                sed::assets::loadFromJKL(validInput, options);
            check(valid.kind == fixture.project
                    && valid.sectors.size() == 1
                    && valid.sectors[0].surfaces.size() == 1
                    && valid.sectors[0].surfaces[0].vertices.size() == 3,
                "JKL.SectorRecords: each supported project retains all corners with valid world vertex references");
        }

        constexpr std::string_view countDiagnostic =
            "Cannot load JKL/NDY sector: declared sector count is invalid or truncated.";
        const auto expectInvalidSectorCount = [
            &makeSectorSectionFixture,
            countDiagnostic](
            const std::string_view records,
            const char* const message)
        {
            checkThrowsWithMessage(
                [&makeSectorSectionFixture, records]()
                {
                    std::istringstream input(makeSectorSectionFixture(1, records));
                    (void)sed::assets::loadFromJKL(input);
                },
                countDiagnostic,
                message);
        };
        expectInvalidSectorCount("",
            "JKL.SectorRecords: missing WORLD SECTORS count is rejected");
        expectInvalidSectorCount("WORLD SECTORS invalid\n",
            "JKL.SectorRecords: malformed WORLD SECTORS count is rejected");
        expectInvalidSectorCount("WORLD SECTORS -1\n",
            "JKL.SectorRecords: negative WORLD SECTORS count is rejected");
        expectInvalidSectorCount(
            "WORLD SECTORS 2\n"
            "SECTOR 0\n"
            "FLAGS 0\n"
            "AMBIENT LIGHT 0\n"
            "EXTRA LIGHT 0\n"
            "BOUNDBOX 0 0 0 0 0 0\n"
            "CENTER 0 0 0\n"
            "RADIUS 0\n"
            "VERTICES 0\n"
            "SURFACES 0 0\n",
            "JKL.SectorRecords: fewer sector records than declared are rejected");

        constexpr std::string_view missingSurfacesDiagnostic =
            "Cannot load JKL/NDY sector: SURFACES record is missing.";
        checkThrowsWithMessage(
            [&makeSectorSectionFixture]()
            {
                const std::string truncated = makeSectorSectionFixture(
                    1,
                    "WORLD SECTORS 1\n"
                    "SECTOR 0\n"
                    "FLAGS 0\n"
                    "AMBIENT LIGHT 0\n"
                    "EXTRA LIGHT 0\n"
                    "BOUNDBOX 0 0 0 0 0 0\n"
                    "CENTER 0 0 0\n"
                    "RADIUS 0\n"
                    "VERTICES 0\n"
                    "");
                std::istringstream input(truncated);
                (void)sed::assets::loadFromJKL(input);
            },
            missingSurfacesDiagnostic,
            "JKL.SectorRecords: a sector truncated before SURFACES is rejected");

        constexpr std::string_view twoEmptyClassicSectors =
            "WORLD SECTORS 2\n"
            "SECTOR 0\n"
            "FLAGS 0\n"
            "AMBIENT LIGHT 0\n"
            "EXTRA LIGHT 0\n"
            "SURFACES 0 0\n"
            "SECTOR 1\n"
            "FLAGS 0\n"
            "AMBIENT LIGHT 0\n"
            "EXTRA LIGHT 0\n"
            "SURFACES 0 0\n";
        constexpr std::string_view twoEmptyIjimSectors =
            "WORLD SECTORS 2\n"
            "SECTOR 0\n"
            "FLAGS 0\n"
            "AMBIENT LIGHT 0 0 0\n"
            "EXTRA LIGHT 0 0 0\n"
            "SURFACES 0 0\n"
            "SECTOR 1\n"
            "FLAGS 0\n"
            "AMBIENT LIGHT 0 0 0\n"
            "EXTRA LIGHT 0 0 0\n"
            "SURFACES 0 0\n";
        for ( const auto& fixture : std::array{
                 std::pair{ 1, twoEmptyClassicSectors },
                 std::pair{ 3, twoEmptyIjimSectors } } )
        {
            const std::string complete = makeSectorSectionFixture(
                fixture.first, fixture.second);
            std::istringstream completeInput(complete);
            const sed::Level completeLevel = sed::assets::loadFromJKL(
                completeInput,
                sed::assets::JklLoadOptions{
                    .projectType = fixture.first == 3
                        ? sed::ProjectType::IJIM
                        : sed::ProjectType::JKDF2 });
            check(completeLevel.sectors.size() == 2,
                "JKL.SectorRecords: complete two-sector classic and IJIM worlds are accepted");

            for ( const std::string_view declaredCount : {
                     std::string_view("1"), std::string_view("0") } )
            {
                std::string underDeclared = makeSectorSectionFixture(
                    fixture.first, fixture.second);
                const std::size_t countPosition = underDeclared.find(
                    "WORLD SECTORS 2");
                check(countPosition != std::string::npos,
                    "JKL.SectorRecords: two-sector fixture count marker is present");
                if ( countPosition == std::string::npos )
                {
                    continue;
                }
                underDeclared.replace(
                    countPosition,
                    std::string_view("WORLD SECTORS 2").size(),
                    "WORLD SECTORS " + std::string(declaredCount));
                checkThrowsWithMessage(
                    [&underDeclared, &fixture]()
                    {
                        std::istringstream input(underDeclared);
                        sed::assets::JklLoadOptions options{};
                        options.projectType = fixture.first == 3
                            ? sed::ProjectType::IJIM
                            : sed::ProjectType::JKDF2;
                        (void)sed::assets::loadFromJKL(input, options);
                    },
                    countDiagnostic,
                    "JKL.SectorRecords: complete sector records beyond the declared count are rejected");
            }
        }

        std::string commentedAndExtended = makeSectorSectionFixture(
            1,
            std::string(twoEmptyClassicSectors)
                + "# retained as a section comment\n"
                + "FUTURE SECTOR FIELD 1\n");
        commentedAndExtended +=
            "SECTION: COGS\n"
            "WORLD COGS 0\n"
            "END\n";
        std::istringstream commentedAndExtendedInput(commentedAndExtended);
        const sed::Level commentedAndExtendedLevel =
            sed::assets::loadFromJKL(commentedAndExtendedInput);
        check(commentedAndExtendedLevel.sectors.size() == 2
                && commentedAndExtendedLevel.cogs.empty(),
            "JKL.SectorRecords: ignored section comments, unknown trailing fields, and later sections do not count as extra sectors");
    }

    {
        struct InvalidSpan
        {
            std::string_view base;
            std::string_view count;
        };
        const std::array<InvalidSpan, 10> invalidSpans{
            InvalidSpan{ "-1", "0" },
            InvalidSpan{ "invalid", "0" },
            InvalidSpan{ "2147483648", "0" },
            InvalidSpan{ "0", "-1" },
            InvalidSpan{ "0", "invalid" },
            InvalidSpan{ "0", "" },
            InvalidSpan{ "0", "2147483648" },
            InvalidSpan{ "1", "0" },
            InvalidSpan{ "0", "1" },
            InvalidSpan{ "2147483647", "2" },
        };
        for ( const InvalidSpan span : invalidSpans )
        {
            std::string diagnostic;
            try
            {
                std::istringstream input(makeEmptySurfaceSpanFixture(span.base, span.count));
                (void)sed::assets::loadFromJKL(input);
            }
            catch (const std::exception& exception)
            {
                diagnostic = exception.what();
            }
            const std::string message = "JKL sector SURFACES span (base='"
                + std::string(span.base) + "', count='" + std::string(span.count)
                + "') has the fixed invalid-span diagnostic";
            check(diagnostic
                    == "Cannot load JKL/NDY sector: SURFACES span is invalid.",
                message.c_str());
        }

        const std::string oneUnusedGlobalSurface = R"JKL(
        SECTION: HEADER
        VERSION 1
        END
        SECTION: GEORESOURCE
        WORLD COLORMAPS 0
        WORLD VERTICES 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        WORLD TEXTURE VERTICES 3
        0: 0 0
        1: 1 0
        2: 0 1
        WORLD ADJOINS 0
        WORLD SURFACES 1
        0: -1 0x0 0x0 0 3 3 -1 0 3 0,0 1,1 2,2 0 0 0
        END
        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0
        AMBIENT LIGHT 0
        EXTRA LIGHT 0
        SURFACES 1 0
        END
        )JKL";
        std::istringstream validEndSpan(oneUnusedGlobalSurface);
        const sed::Level empty = sed::assets::loadFromJKL(validEndSpan);
        check(empty.sectors.size() == 1 && empty.sectors[0].surfaces.empty(),
            "JKL sector SURFACES span: zero-length span at the nonempty global surface end is valid");
    }

    {
        struct ColorToken
        {
            std::string_view token;
            std::string_view diagnostic;
        };
        const std::array<ColorToken, 32> colorTokens{
            ColorToken{ "11.1", "Cannot load JKL/NDY fog: a color value is invalid or outside float range." },
            ColorToken{ "11.2", "Cannot load JKL/NDY fog: a color value is invalid or outside float range." },
            ColorToken{ "11.3", "Cannot load JKL/NDY fog: a color value is invalid or outside float range." },
            ColorToken{ "11.4", "Cannot load JKL/NDY fog: a color value is invalid or outside float range." },
            ColorToken{ "21.1", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "21.2", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "21.3", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "22.1", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "22.2", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "22.3", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "23.1", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "23.2", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "23.3", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "24.1", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "24.2", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "24.3", "Cannot load JKL/NDY sector light: a color value is invalid or outside float range." },
            ColorToken{ "25.1", "Cannot load JKL/NDY extra light: a color value is invalid or outside float range." },
            ColorToken{ "25.2", "Cannot load JKL/NDY extra light: a color value is invalid or outside float range." },
            ColorToken{ "25.3", "Cannot load JKL/NDY extra light: a color value is invalid or outside float range." },
            ColorToken{ "25.4", "Cannot load JKL/NDY extra light: a color value is invalid or outside float range." },
            ColorToken{ "26.1", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "26.2", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "26.3", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "26.4", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "26.5", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "26.6", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "26.7", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "26.8", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "26.9", "Cannot load JKL/NDY surface: a color value is invalid or outside float range." },
            ColorToken{ "27.1", "Cannot load JKL/NDY editor light: a color value is invalid or outside float range." },
            ColorToken{ "27.2", "Cannot load JKL/NDY editor light: a color value is invalid or outside float range." },
            ColorToken{ "27.3", "Cannot load JKL/NDY editor light: a color value is invalid or outside float range." },
        };
        const auto checkColorToken = [&](
            std::string fixture,
            const ColorToken& colorToken,
            const std::string_view invalid)
        {
            const std::size_t position = fixture.find(colorToken.token);
            if ( position != std::string::npos )
            {
                fixture.replace(position, colorToken.token.size(), invalid);
            }
            std::string diagnostic;
            try
            {
                std::istringstream input(fixture);
                sed::assets::JklLoadOptions options{};
                options.projectType = sed::ProjectType::IJIM;
                (void)sed::assets::loadFromJKL(input, options);
            }
            catch (const std::exception& exception)
            {
                diagnostic = exception.what();
            }
            const std::string message = "JKL/NDY IJIM ColorF token '"
                + std::string(colorToken.token) + "' replaced by '"
                + std::string(invalid) + "' must report: "
                + std::string(colorToken.diagnostic);
            check(position != std::string::npos && diagnostic == colorToken.diagnostic,
                message.c_str());
        };
        for ( const ColorToken& colorToken : colorTokens )
        {
            checkColorToken(makeIjimColorFixture(), colorToken, "1e39");
        }

        const std::array<ColorToken, 6> colorRepresentatives{
            colorTokens[0],
            colorTokens[4],
            colorTokens[13],
            colorTokens[16],
            colorTokens[20],
            colorTokens[29],
        };
        for ( const ColorToken& colorToken : colorRepresentatives )
        {
            for ( const std::string_view invalid : {
                    "malformed", "inf", "+-1", "++1", "+-0" } )
            {
                checkColorToken(makeIjimColorFixture(), colorToken, invalid);
            }
        }

        struct CornerToken
        {
            std::string_view token;
            bool mots;
        };
        const std::array<CornerToken, 15> version1CornerTokens{
            CornerToken{ "31.1", false },
            CornerToken{ "31.2", false },
            CornerToken{ "31.3", false },
            CornerToken{ "32.1", true },
            CornerToken{ "32.2", true },
            CornerToken{ "32.3", true },
            CornerToken{ "32.4", true },
            CornerToken{ "32.5", true },
            CornerToken{ "32.6", true },
            CornerToken{ "32.7", true },
            CornerToken{ "32.8", true },
            CornerToken{ "32.9", true },
            CornerToken{ "33.1", true },
            CornerToken{ "33.2", true },
            CornerToken{ "33.3", true },
        };
        const std::string classicCorners = "31.1 31.2 31.3";
        const std::string motsCorners =
            "32.1 32.2 32.3 32.4 32.5 32.6 32.7 32.8 32.9 33.1 33.2 33.3";
        for ( const CornerToken& corner : version1CornerTokens )
        {
            std::string fixture = makeVersion1SurfaceColorFixture(
                corner.mots ? motsCorners : classicCorners);
            const std::size_t position = fixture.find(corner.token);
            if ( position != std::string::npos )
            {
                fixture.replace(position, corner.token.size(), "1e39");
            }
            std::string diagnostic;
            try
            {
                std::istringstream input(fixture);
                sed::assets::JklLoadOptions options{};
                options.projectType = corner.mots
                    ? sed::ProjectType::MOTS : sed::ProjectType::JKDF2;
                (void)sed::assets::loadFromJKL(input, options);
            }
            catch (const std::exception& exception)
            {
                diagnostic = exception.what();
            }
            check(position != std::string::npos
                    && diagnostic
                        == "Cannot load JKL/NDY surface: a color value is invalid or outside float range.",
                (std::string(corner.mots ? "MotS" : "JKDF2")
                    + " surface corner token '" + std::string(corner.token)
                    + "' replaced by '1e39' has the fixed rejection diagnostic").c_str());
        }

        for ( const bool mots : { false, true } )
        {
            const std::string project = mots ? "MotS" : "JKDF2";
            for ( const std::string_view invalid : { "malformed", "inf" } )
            {
                const std::string fixture = makeVersion1SurfaceColorFixture(
                    mots
                        ? std::string(invalid) + " 0 0 0 0 0 0 0 0 0 0 0"
                        : std::string(invalid) + " 0 0");
                std::string diagnostic;
                try
                {
                    std::istringstream input(fixture);
                    sed::assets::JklLoadOptions options{};
                    options.projectType = mots
                        ? sed::ProjectType::MOTS : sed::ProjectType::JKDF2;
                    (void)sed::assets::loadFromJKL(input, options);
                }
                catch (const std::exception& exception)
                {
                    diagnostic = exception.what();
                }
                const std::string message = project + " version-1 corner token '"
                    + std::string(invalid)
                    + "' must reject with the fixed surface-color diagnostic";
                check(diagnostic
                        == "Cannot load JKL/NDY surface: a color value is invalid or outside float range.",
                    message.c_str());
            }
        }

        const float largestFinite = std::numeric_limits<float>::max();
        std::istringstream classicMaxInput(makeVersion1SurfaceColorFixture(
            "3.4028235e38 -3.4028235e38 10"));
        std::optional<sed::Level> classicMax;
        try
        {
            classicMax = sed::assets::loadFromJKL(classicMaxInput);
        }
        catch (const std::exception&)
        {
        }
        check(classicMax.has_value()
                && classicMax->sectors.size() == 1
                && classicMax->sectors[0].surfaces.size() == 1
                && classicMax->sectors[0].surfaces[0].texVertices[0].color.a == largestFinite
                && classicMax->sectors[0].surfaces[0].texVertices[1].color.a == -largestFinite,
            "JKDF2 surface intensity: signed and maximum finite float values remain accepted");

        std::istringstream motsMaxInput(makeVersion1SurfaceColorFixture(
            "3.4028235e38 -2 10 -3 4 -5 6 -7 8 -9 10 -11"));
        sed::assets::JklLoadOptions motsMaxOptions{};
        motsMaxOptions.projectType = sed::ProjectType::MOTS;
        std::optional<sed::Level> motsMax;
        try
        {
            motsMax = sed::assets::loadFromJKL(motsMaxInput, motsMaxOptions);
        }
        catch (const std::exception&)
        {
        }
        check(motsMax.has_value()
                && motsMax->sectors.size() == 1
                && motsMax->sectors[0].surfaces.size() == 1
                && motsMax->sectors[0].surfaces[0].texVertices[0].color.a == largestFinite
                && motsMax->sectors[0].surfaces[0].texVertices[0].color.r == -2.0f,
            "MotS surface RGBA: signed and maximum finite float values remain accepted");

        const std::string maxFloat = "3.4028235e38";
        std::istringstream maxColorInput(makeIjimColorFixture(
            maxFloat + " -2 10 -3.5",
            maxFloat + " -2 10",
            maxFloat + " -2 10",
            maxFloat + " -2 10",
            maxFloat + " -2 10",
            maxFloat + " -2 10 0.75",
            maxFloat + " -2 10 -3 4 -5 6 -7 8",
            maxFloat + " -2 10"));
        sed::assets::JklLoadOptions maxColorOptions{};
        maxColorOptions.projectType = sed::ProjectType::IJIM;
        std::optional<sed::Level> maxColor;
        try
        {
            maxColor = sed::assets::loadFromJKL(maxColorInput, maxColorOptions);
        }
        catch (const std::exception&)
        {
        }
        check(maxColor.has_value()
                && maxColor->header.fog.color.r == largestFinite
                && maxColor->header.fog.color.a == -3.5f,
            "JKL/NDY ColorF: maximum finite fog channels and signed color values remain accepted");
        check(maxColor.has_value()
                && maxColor->sectors.size() == 1
                && maxColor->sectors[0].ambient.r == largestFinite
                && maxColor->sectors[0].extraLight.r == largestFinite
                && maxColor->sectors[0].pointLight.color.r == largestFinite
                && maxColor->sectors[0].tint.r == largestFinite,
            "NDY sector ColorF: maximum finite ambient, extra, average, and tint values remain accepted");
        check(maxColor.has_value()
                && maxColor->sectors.size() == 1
                && maxColor->sectors[0].surfaces.size() == 1
                && maxColor->sectors[0].surfaces[0].extraLight.r == largestFinite
                && maxColor->sectors[0].surfaces[0].texVertices.size() == 3
                && maxColor->sectors[0].surfaces[0].texVertices[0].color.r == largestFinite
                && maxColor->lights.size() == 1
                && maxColor->lights[0].color.r == largestFinite,
            "NDY surface and editor-light ColorF: maximum finite channels remain accepted");
    }

    {
        // Classic JKL stores sector ambient and extra light as signed scalar
        // values. Import must not apply the renderer's display clamp to finite
        // authored values, including negative and HDR values.
        struct ScalarVector
        {
            const char* wire;
            double expected;
        };
        for (const ScalarVector vector : {
                 ScalarVector{ "-2.5", -2.5 },
                 ScalarVector{ "-0.0", 0.0 },
                 ScalarVector{ "0", 0.0 },
                 ScalarVector{ "1", 1.0 },
                 ScalarVector{ "1.08", 1.08 },
                 ScalarVector{ "10.0", 10.0 },
                 ScalarVector{ "3e38", static_cast<double>(3.0e38f) },
             })
        {
            std::istringstream input(makeSectorLightingFixture(
                1, vector.wire, vector.wire));
            const sed::Level level = sed::assets::loadFromJKL(input);
            check(level.sectors.size() == 1,
                "JKL sector light: scalar fixture loads one sector");
            if (level.sectors.size() == 1)
            {
                const sed::Sector& sector = level.sectors[0];
                check(close(sector.ambient.r, vector.expected)
                        && close(sector.ambient.g, vector.expected)
                        && close(sector.ambient.b, vector.expected)
                        && close(sector.ambient.a, 0.0),
                    "JKL sector light: ambient scalar preserves fixed wire value");
                check(close(sector.extraLight.r, vector.expected)
                        && close(sector.extraLight.g, vector.expected)
                        && close(sector.extraLight.b, vector.expected)
                        && close(sector.extraLight.a, 0.0),
                    "JKL sector light: extra scalar preserves fixed wire value");
            }
        }

        for (const std::string_view invalid : { "nan", "inf", "-inf", "1e39", "1e400" })
        {
            for (const bool extraField : { false, true })
            {
                std::string diagnostic;
                try
                {
                    const std::string_view ambient = extraField
                        ? std::string_view{ "0" } : invalid;
                    const std::string_view extra = extraField
                        ? invalid : std::string_view{ "0" };
                    std::istringstream input(makeSectorLightingFixture(
                        1, ambient, extra));
                    (void)sed::assets::loadFromJKL(input);
                }
                catch (const std::exception& exception)
                {
                    diagnostic = exception.what();
                }
                check(diagnostic
                        == "Cannot load JKL/NDY sector light: a color value is invalid or outside float range.",
                    "JKL sector light: nonfinite scalar has the fixed diagnostic");
            }
        }
    }

    {
        // IJIM has no engine tint record by default. Keep that absent state
        // distinct from the editor's newly-created sector tint default so a
        // loaded level does not acquire a synthetic TINT 0 0 0 record.
        std::istringstream absentInput(makeSectorLightingFixture(
            3, "0.25 0.5 10.0", "-0.25 1.08 2.0"));
        sed::assets::JklLoadOptions absentOptions{};
        absentOptions.projectType = sed::ProjectType::IJIM;
        const sed::Level absent = sed::assets::loadFromJKL(absentInput, absentOptions);
        check(absent.sectors.size() == 1,
            "IJIM sector tint: absent-TINT fixture loads one sector");
        if (absent.sectors.size() == 1)
        {
            const sed::Sector& sector = absent.sectors[0];
            check(sector.tint.r == 0.0f && sector.tint.g == 0.0f
                    && sector.tint.b == 0.0f && sector.tint.a == 0.0f,
                "IJIM sector tint: absent TINT uses all-zero engine RGB default and zero editor alpha");

            std::ostringstream exported;
            sed::assets::saveToJKL(absent, exported);
            check(exported.str().find("TINT ") == std::string::npos,
                "IJIM sector tint: absent TINT is not synthesized on export");
        }

        std::istringstream explicitInput(makeSectorLightingFixture(
            3, "0", "0", "0.00000010 0 0"));
        sed::assets::JklLoadOptions explicitOptions{};
        explicitOptions.projectType = sed::ProjectType::IJIM;
        const sed::Level explicitTint = sed::assets::loadFromJKL(explicitInput, explicitOptions);
        check(explicitTint.sectors.size() == 1,
            "IJIM sector tint: explicit-TINT fixture loads one sector");
        if (explicitTint.sectors.size() == 1)
        {
            const sed::ColorF& tint = explicitTint.sectors[0].tint;
            check(tint.r == 0.00000010f && tint.g == 0.0f
                    && tint.b == 0.0f,
                "IJIM sector tint: explicit RGB preserves fixed wire values");
            std::ostringstream exported;
            sed::assets::saveToJKL(explicitTint, exported);
            check(exported.str().find("TINT 0.00000010 0.00000000 0.00000000")
                    != std::string::npos,
                "IJIM sector tint: explicit small RGB remains serialized");
        }

        sed::Level newLevel{};
        newLevel.clear();
        newLevel.kind = sed::ProjectType::IJIM;
        const sed::Sector& newSector = newLevel.newSector();
        check(newSector.tint == sed::ColorWhite,
            "IJIM sector tint: newly-created sector keeps editor default");
    }

    {
        // Authored classic extra light is signed and may exceed the renderer's
        // display range. The JKL wire value must survive load unchanged.
        const char* jkl = R"JKL(
        SECTION: HEADER
        VERSION 1
        END
        SECTION: MATERIALS
        WORLD MATERIALS 0
        END
        SECTION: GEORESOURCE
        WORLD COLORMAPS 0
        WORLD VERTICES 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        WORLD TEXTURE VERTICES 3
        0: 0 0
        1: 1 0
        2: 0 1
        WORLD ADJOINS 0
        WORLD SURFACES 1
        0: -1 0x0 0x0 0 3 3 -1 10 3 0,0 1,1 2,2 0 0 0
        0: 0 0 1
        END
        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0
        AMBIENT LIGHT 0
        EXTRA LIGHT 0
        VERTICES 3
        0: 0
        1: 1
        2: 2
        SURFACES 0 1
        END
        )JKL";
        std::istringstream input(jkl);
        const sed::Level level = sed::assets::loadFromJKL(input);
        check(level.sectors.size() == 1 && level.sectors[0].surfaces.size() == 1,
              "JKL HDR: surface fixture loads");
        if (!level.sectors.empty() && !level.sectors[0].surfaces.empty())
        {
            const sed::ColorF& extra = level.sectors[0].surfaces[0].extraLight;
            check(close(extra.r, 10.0), "JKL HDR: extraLight.r +10 survives load");
            check(close(extra.g, 10.0), "JKL HDR: extraLight.g +10 survives load");
            check(close(extra.b, 10.0), "JKL HDR: extraLight.b +10 survives load");
            check(close(extra.a, 1.0), "JKL HDR: extraLight.a remains 1");

            sed::assets::JklSaveOptions preserveHdr;
            preserveHdr.clampExtraLight = false;
            std::ostringstream exported;
            sed::assets::saveToJKL(level, exported, preserveHdr);
            const std::size_t surfaceHeader = exported.str().find("WORLD SURFACES ");
            const std::size_t surfaceLineStart = surfaceHeader == std::string::npos
                ? std::string::npos : exported.str().find('\n', surfaceHeader);
            const std::size_t surfaceLineEnd = surfaceLineStart == std::string::npos
                ? std::string::npos : exported.str().find('\n', surfaceLineStart + 1);
            std::istringstream surfaceTokens(
                surfaceLineStart == std::string::npos || surfaceLineEnd == std::string::npos
                    ? std::string{} : exported.str().substr(surfaceLineStart + 1, surfaceLineEnd - surfaceLineStart - 1));
            std::string surfaceIndex;
            std::string token;
            double wireIntensity = 0.0;
            surfaceTokens >> surfaceIndex;
            for (int field = 0; field < 7; ++field) surfaceTokens >> token;
            const bool parsedWireIntensity = static_cast<bool>(surfaceTokens >> wireIntensity);
            check(parsedWireIntensity && close(wireIntensity, 10.0),
                "JKL HDR: emitted scalar wire token is independently parsed");
            std::istringstream roundTripInput(exported.str());
            const sed::Level roundTrip = sed::assets::loadFromJKL(roundTripInput);
            check(!roundTrip.sectors.empty() && !roundTrip.sectors[0].surfaces.empty(),
                "JKL HDR: exported level reloads with a surface");
            if (!roundTrip.sectors.empty() && !roundTrip.sectors[0].surfaces.empty())
            {
                const sed::ColorF& roundTripExtra = roundTrip.sectors[0].surfaces[0].extraLight;
                check(close(roundTripExtra.r, 10.0) && close(roundTripExtra.g, 10.0)
                        && close(roundTripExtra.b, 10.0) && close(roundTripExtra.a, 1.0),
                    "JKL HDR: exported scalar preserves all extraLight channels");
            }

            for (const float expected : { -2.0f, 2.0f })
            {
                std::string variant(jkl);
                const std::size_t marker = variant.find("10 3");
                variant.replace(marker, 4, std::to_string(expected) + " 3");
                std::istringstream variantInput(variant);
                const sed::Level variantLevel = sed::assets::loadFromJKL(variantInput);
                check(!variantLevel.sectors.empty() && !variantLevel.sectors[0].surfaces.empty(),
                    "JKL HDR: signed scalar vector fixture reloads");
                if (!variantLevel.sectors.empty() && !variantLevel.sectors[0].surfaces.empty())
                {
                    const sed::ColorF& variantExtra = variantLevel.sectors[0].surfaces[0].extraLight;
                    check(close(variantExtra.r, expected) && close(variantExtra.g, expected)
                            && close(variantExtra.b, expected) && close(variantExtra.a, 1.0),
                        "JKL HDR: signed scalar vector survives load");
                }
            }

            for (const std::string& invalid : { "not-a-number", "nan", "inf", "1e400", "1e39" })
            {
                std::string malformed(jkl);
                const std::size_t malformedMarker = malformed.find("10 3");
                malformed.replace(malformedMarker, 4, invalid + " 3");
                bool rejected = false;
                try
                {
                    std::istringstream malformedInput(malformed);
                    (void)sed::assets::loadFromJKL(malformedInput);
                }
                catch (const std::runtime_error& exception)
                {
                    const std::string message = exception.what();
                    rejected = message
                        == "Cannot load JKL/NDY extra light: a color value is invalid or outside float range.";
                }
                check(rejected, "JKL HDR: invalid extraLight token has the fixed diagnostic");
            }

            std::string motsText(jkl);
            const std::string cornerLights = "1 2 3 4 1 2 3 4 1 2 3 4\n        0: 0 0 1";
            const std::size_t cornerMarker = motsText.find("0 0 0\n        0: 0 0 1");
            motsText.replace(cornerMarker, std::string("0 0 0\n        0: 0 0 1").size(), cornerLights);
            std::istringstream motsInput(motsText);
            sed::assets::JklLoadOptions motsOptions{};
            motsOptions.projectType = sed::ProjectType::MOTS;
            const sed::Level motsLevel = sed::assets::loadFromJKL(motsInput, motsOptions);
            check(!motsLevel.sectors.empty() && !motsLevel.sectors[0].surfaces.empty(),
                "MotS HDR: fixture reloads");
            if (!motsLevel.sectors.empty() && !motsLevel.sectors[0].surfaces.empty())
            {
                const sed::ColorF& motsExtra = motsLevel.sectors[0].surfaces[0].extraLight;
                check(close(motsExtra.r, 10.0) && close(motsExtra.g, 10.0)
                        && close(motsExtra.b, 10.0) && close(motsExtra.a, 1.0),
                    "MotS HDR: signed scalar survives load");
            }

            const char* ijimText = R"NDY(
            SECTION: HEADER
            VERSION 3
            END
            SECTION: MATERIALS
            WORLD MATERIALS 0
            END
            SECTION: GEORESOURCE
            WORLD VERTICES 3
            0: 0 0 0
            1: 1 0 0
            2: 0 1 0
            WORLD TEXTURE VERTICES 3
            0: 0 0
            1: 1 0
            2: 0 1
            WORLD ADJOINS 0
            WORLD SURFACES 1
            0: -1 0x0 0x0 0 3 3 -1 10 -2 2 0.75 3 0,0 1,1 2,2 1 2 3 4 5 6 7 8 9
            0: 0 0 1
            END
            SECTION: SECTORS
            WORLD SECTORS 1
            SECTOR 0
            FLAGS 0
            AMBIENT LIGHT 0 0 0
            EXTRA LIGHT 0 0 0
            VERTICES 3
            0: 0
            1: 1
            2: 2
            SURFACES 0 1
            END
            )NDY";
            std::istringstream ijimInput(ijimText);
            sed::assets::JklLoadOptions ijimOptions{};
            ijimOptions.projectType = sed::ProjectType::IJIM;
            const sed::Level ijimLevel = sed::assets::loadFromJKL(ijimInput, ijimOptions);
            check(!ijimLevel.sectors.empty() && !ijimLevel.sectors[0].surfaces.empty(),
                "IJIM HDR: fixture reloads");
            if (!ijimLevel.sectors.empty() && !ijimLevel.sectors[0].surfaces.empty())
            {
                const sed::ColorF& ijimExtra = ijimLevel.sectors[0].surfaces[0].extraLight;
                check(close(ijimExtra.r, 10.0) && close(ijimExtra.g, -2.0)
                        && close(ijimExtra.b, 2.0) && close(ijimExtra.a, 0.75),
                    "IJIM HDR: RGB and alpha vector survives load");
            }
            std::ostringstream ijimExport;
            sed::assets::saveToJKL(ijimLevel, ijimExport, preserveHdr);
            const std::size_t ijimSurfaceHeader = ijimExport.str().find("WORLD SURFACES ");
            const std::size_t ijimLineStart = ijimSurfaceHeader == std::string::npos
                ? std::string::npos : ijimExport.str().find('\n', ijimSurfaceHeader);
            const std::size_t ijimLineEnd = ijimLineStart == std::string::npos
                ? std::string::npos : ijimExport.str().find('\n', ijimLineStart + 1);
            std::istringstream ijimSurfaceTokens(
                ijimLineStart == std::string::npos || ijimLineEnd == std::string::npos
                    ? std::string{} : ijimExport.str().substr(ijimLineStart + 1, ijimLineEnd - ijimLineStart - 1));
            std::string ijimIndex;
            std::string ijimToken;
            double wireR = 0.0;
            double wireG = 0.0;
            double wireB = 0.0;
            double wireA = 0.0;
            ijimSurfaceTokens >> ijimIndex;
            for (int field = 0; field < 7; ++field) ijimSurfaceTokens >> ijimToken;
            const bool parsedWireColor = static_cast<bool>(
                ijimSurfaceTokens >> wireR >> wireG >> wireB >> wireA);
            check(parsedWireColor && close(wireR, 10.0) && close(wireG, -2.0)
                    && close(wireB, 2.0) && close(wireA, 0.75),
                "IJIM HDR: emitted RGB wire tokens are independently parsed");

            sed::Level extreme = level;
            extreme.sectors[0].surfaces[0].extraLight = {
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                1.0f };
            std::ostringstream extremeOutput;
            sed::assets::saveToJKL(extreme, extremeOutput, preserveHdr);
            check(extremeOutput.str().find("inf") == std::string::npos
                    && extremeOutput.str().find("nan") == std::string::npos,
                "JKL HDR: finite extreme channels export without inf or nan");
            std::istringstream extremeInput(extremeOutput.str());
            const sed::Level extremeRoundTrip = sed::assets::loadFromJKL(extremeInput);
            check(!extremeRoundTrip.sectors.empty() && !extremeRoundTrip.sectors[0].surfaces.empty(),
                "JKL HDR: finite extreme export reloads");
            if (!extremeRoundTrip.sectors.empty() && !extremeRoundTrip.sectors[0].surfaces.empty())
            {
                check(close(extremeRoundTrip.sectors[0].surfaces[0].extraLight.r,
                        std::numeric_limits<float>::max(), 1e-5),
                    "JKL HDR: finite extreme scalar remains representable");
            }

            extreme.sectors[0].surfaces[0].extraLight.r = std::numeric_limits<float>::quiet_NaN();
            bool rejectedNonfiniteExport = false;
            try
            {
                std::ostringstream invalidOutput;
                sed::assets::saveToJKL(extreme, invalidOutput);
            }
            catch (const std::runtime_error& exception)
            {
                rejectedNonfiniteExport = std::string(exception.what())
                    == "Cannot export JKL/NDY extra light: a color value is not finite.";
            }
            check(rejectedNonfiniteExport, "JKL HDR: nonfinite extraLight export has the fixed diagnostic");

            const auto checkNonfiniteExport = [](const sed::Level& source, const char* label)
            {
                if (source.sectors.empty() || source.sectors[0].surfaces.empty())
                {
                    check(false, label);
                    return;
                }
                for (const int channel : { 0, 1, 2, 3 })
                {
                    sed::Level invalid = source;
                    sed::ColorF& color = invalid.sectors[0].surfaces[0].extraLight;
                    float* component = channel == 0 ? &color.r
                        : channel == 1 ? &color.g : channel == 2 ? &color.b : &color.a;
                    *component = std::numeric_limits<float>::quiet_NaN();
                    for (const bool clampExtraLight : { true, false })
                    {
                        std::ostringstream invalidOutput;
                        invalidOutput << "sentinel";
                        bool rejected = false;
                        sed::assets::JklSaveOptions options;
                        options.clampExtraLight = clampExtraLight;
                        try
                        {
                            sed::assets::saveToJKL(invalid, invalidOutput, options);
                        }
                        catch (const std::runtime_error& exception)
                        {
                            rejected = std::string(exception.what())
                                == "Cannot export JKL/NDY extra light: a color value is not finite.";
                        }
                        check(rejected && invalidOutput.str() == "sentinel", label);
                    }
                }
            };
            checkNonfiniteExport(level, "JKL HDR: every nonfinite RGBA channel is rejected atomically");
            checkNonfiniteExport(motsLevel, "MotS HDR: every nonfinite RGBA channel is rejected atomically");
            checkNonfiniteExport(ijimLevel, "IJIM HDR: every nonfinite RGBA channel is rejected atomically");
        }
    }

    {
        // JKL COG surface arguments address GEORESOURCE surface records.  The
        // sector SURFACES spans are allowed to present those records in an
        // order different from the editor's flattened sector order.  Both the
        // immediate resource-backed path and the deferred finalizer must keep
        // the same physical face identity, including after native save/load
        // and JKL re-export.
        const auto unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root = "tmp_jkl_cog_surface_order_" + unique;
        std::error_code ec;
        std::filesystem::create_directories(root / "cog", ec);
        check(!ec, "JKL.COGSurfaceOrder: resource directory creates");
        {
            std::ofstream out(root / "cog" / "target.cog");
            out << "symbols\n"
                << "surface target\n"
                << "end\n";
        }
        {
            std::ofstream out(root / "cog" / "default_surface.cog");
            out << "symbols\n"
                << "surface target=0\n"
                << "end\n";
        }

        const char* jkl = R"JKL(SECTION: HEADER
VERSION 1
END
SECTION: MATERIALS
WORLD MATERIALS 0
END
SECTION: GEORESOURCE
WORLD COLORMAPS 0
WORLD VERTICES 6
0: 0 0 0
1: 1 0 0
2: 0 1 0
3: 0 0 1
4: 1 0 1
5: 0 1 1
WORLD TEXTURE VERTICES 6
0: 0 0
1: 1 0
2: 0 1
3: 0 0
4: 1 0
5: 0 1
WORLD ADJOINS 0
WORLD SURFACES 2
0: -1 0x0 0x0 0 3 3 -1 0 3 0,0 1,1 2,2
1: -1 0x0 0x0 0 3 3 -1 0 3 3,3 4,4 5,5
END
SECTION: SECTORS
WORLD SECTORS 2
SECTOR 0
FLAGS 0
AMBIENT LIGHT 0
EXTRA LIGHT 0
VERTICES 3
0: 3
1: 4
2: 5
SURFACES 1 1
SECTOR 1
FLAGS 0
AMBIENT LIGHT 0
EXTRA LIGHT 0
VERTICES 3
0: 0
1: 1
2: 2
SURFACES 0 1
END
SECTION: COGS
WORLD COGS 1
0: target.cog 0
END
)JKL";

        sed::assets::ResourceLocator resources{};
        resources.addRoot(root);
        sed::assets::JklLoadOptions immediateOptions{};
        immediateOptions.resources = &resources;

        std::istringstream immediateInput(jkl);
        const sed::Level immediate = sed::assets::loadFromJKL(immediateInput, immediateOptions);
        const auto checkSurfaceTarget = [](const sed::Level& level)
        {
            return level.cogs.size() == 1
                && level.cogs[0].values.size() == 1
                && level.cogs[0].values[0].intValue == 1
                && level.cogs[0].values[0].asJedString(level) == "1 0";
        };
        check(checkSurfaceTarget(immediate),
            "JKL.COGSurfaceOrder: immediate resource finalization preserves wire target identity");

        std::string defaultSurfaceJkl(jkl);
        const std::size_t defaultSurfaceRow = defaultSurfaceJkl.find(
            "target.cog 0");
        check(defaultSurfaceRow != std::string::npos,
            "JKL.COGSurfaceOrder: default-surface fixture row anchor exists");
        if (defaultSurfaceRow != std::string::npos)
        {
            defaultSurfaceJkl.replace(
                defaultSurfaceRow, std::string_view("target.cog 0").size(),
                "default_surface.cog");
            std::istringstream defaultSurfaceInput(defaultSurfaceJkl);
            const sed::Level defaultSurface = sed::assets::loadFromJKL(
                defaultSurfaceInput, immediateOptions);
            check(defaultSurface.cogs.size() == 1
                    && defaultSurface.cogs[0].values.size() == 1
                    && defaultSurface.cogs[0].values[0].intValue == 1
                    && defaultSurface.cogs[0].values[0].asJedString(defaultSurface)
                        == "1 0",
                "JKL.COGSurfaceOrder: omitted surface default follows the GEORESOURCE map");
        }

        std::string hexadecimalJkl(jkl);
        const std::size_t decimalTarget = hexadecimalJkl.find("target.cog 0");
        check(decimalTarget != std::string::npos,
            "JKL.COGSurfaceOrder: hexadecimal fixture replacement anchor exists");
        if (decimalTarget != std::string::npos)
        {
            hexadecimalJkl.replace(decimalTarget, std::string_view("target.cog 0").size(),
                "target.cog 0x0");
            std::istringstream hexadecimalInput(hexadecimalJkl);
            const sed::Level hexadecimal = sed::assets::loadFromJKL(
                hexadecimalInput, immediateOptions);
            check(checkSurfaceTarget(hexadecimal),
                "JKL.COGSurfaceOrder: hexadecimal wire target uses the same surface map");
        }

        const std::vector<std::byte> saved = sed::assets::saveToSedBytes(immediate);
        const sed::Level nativeRoundTrip = sed::assets::loadFromSedBytes(saved);
        check(checkSurfaceTarget(nativeRoundTrip),
            "JKL.COGSurfaceOrder: native SED round-trip preserves the corrected target");

        sed::assets::JklLoadOptions deferredOptions{};
        std::istringstream deferredInput(jkl);
        sed::Level deferred = sed::assets::loadFromJKL(deferredInput, deferredOptions);
        check(deferred.cogs.size() == 1 && deferred.cogs[0].values.size() == 1
                && deferred.cogs[0].values[0].stringValue == "0",
            "JKL.COGSurfaceOrder: deferred load retains the raw wire argument");
        {
            std::ostringstream unresolvedNative;
            unresolvedNative << "sentinel";
            bool rejectedNative = false;
            try
            {
                (void)sed::assets::saveToSedBytes(deferred);
            }
            catch (const std::runtime_error& exception)
            {
                rejectedNative = std::string(exception.what())
                    == "Cannot save SED: imported COG surface references require resource-backed COG script 'target.cog' before serialization.";
            }
            check(rejectedNative && unresolvedNative.str() == "sentinel",
                "JKL.COGSurfaceOrder: unresolved nonidentity import is rejected before native serialization");

            std::ostringstream unresolvedExport;
            unresolvedExport << "sentinel";
            bool rejectedExport = false;
            try
            {
                sed::assets::saveToJKL(deferred, unresolvedExport);
            }
            catch (const std::runtime_error& exception)
            {
                rejectedExport = std::string(exception.what())
                    == "Cannot export JKL/NDY: imported COG surface references require resource-backed COG script 'target.cog' before serialization.";
            }
            check(rejectedExport && unresolvedExport.str() == "sentinel",
                "JKL.COGSurfaceOrder: unresolved nonidentity import is rejected before engine serialization");
        }

        std::istringstream unresolvedFinalizeInput(jkl);
        sed::Level unresolvedFinalize = sed::assets::loadFromJKL(
            unresolvedFinalizeInput, deferredOptions);
        sed::assets::ResourceLocator missingResources{};
        bool rejectedFinalize = false;
        try
        {
            sed::assets::finalizeJklImportResources(
                unresolvedFinalize, missingResources);
        }
        catch (const std::runtime_error& exception)
        {
            rejectedFinalize = std::string(exception.what())
                == "Cannot finalize JKL/NDY: imported COG surface references require resource-backed COG script 'target.cog' before publishing the level.";
        }
        check(rejectedFinalize,
            "JKL.COGSurfaceOrder: unresolved nonidentity import is rejected before publication");

        std::string missingEmptyRowJkl(jkl);
        const std::size_t missingEmptyRow = missingEmptyRowJkl.find(
            "target.cog 0");
        check(missingEmptyRow != std::string::npos,
            "JKL.COGSurfaceOrder: missing empty-row fixture anchor exists");
        if (missingEmptyRow != std::string::npos)
        {
            missingEmptyRowJkl.replace(
                missingEmptyRow, std::string_view("target.cog 0").size(),
                "missing_empty.cog");
            std::istringstream missingEmptyInput(missingEmptyRowJkl);
            sed::Level missingEmpty = sed::assets::loadFromJKL(
                missingEmptyInput, deferredOptions);
            bool rejectedMissingEmpty = false;
            try
            {
                sed::assets::finalizeJklImportResources(
                    missingEmpty, missingResources);
            }
            catch (const std::runtime_error& exception)
            {
                rejectedMissingEmpty = std::string(exception.what())
                    == "Cannot finalize JKL/NDY: imported COG surface references require resource-backed COG script 'missing_empty.cog' before publishing the level.";
            }
            check(rejectedMissingEmpty,
                "JKL.COGSurfaceOrder: unresolved empty row is rejected before publication");
        }

        // A later missing schema must not partially publish an earlier
        // successfully resolved COG.  The finalizer stages all hydrated
        // values and reports the unresolved script before mutating the level.
        std::string mixedJkl(jkl);
        const std::size_t mixedCount = mixedJkl.find("WORLD COGS 1");
        check(mixedCount != std::string::npos,
            "JKL.COGSurfaceOrder: mixed-schema fixture count anchor exists");
        if (mixedCount != std::string::npos)
        {
            mixedJkl.replace(mixedCount, std::string_view("WORLD COGS 1").size(),
                "WORLD COGS 2");
        }
        const std::string_view mixedRowAnchor = "0: target.cog 0\nEND\n";
        const std::size_t mixedRow = mixedJkl.find(mixedRowAnchor);
        check(mixedRow != std::string::npos,
            "JKL.COGSurfaceOrder: mixed-schema fixture row anchor exists");
        if (mixedRow != std::string::npos)
        {
            mixedJkl.replace(mixedRow, mixedRowAnchor.size(),
                "0: target.cog 0\n1: missing.cog 0\nEND\n");
        }
        if (mixedCount != std::string::npos && mixedRow != std::string::npos)
        {
            std::istringstream mixedInput(mixedJkl);
            sed::Level mixed = sed::assets::loadFromJKL(mixedInput, deferredOptions);
            bool rejectedMixed = false;
            try
            {
                sed::assets::finalizeJklImportResources(mixed, resources);
            }
            catch (const std::runtime_error& exception)
            {
                rejectedMixed = std::string(exception.what())
                    == "Cannot finalize JKL/NDY: imported COG surface references require resource-backed COG script 'missing.cog' before publishing the level.";
            }
            check(rejectedMixed,
                "JKL.COGSurfaceOrder: mixed loaded/missing schemas report the unresolved script");
            check(mixed.cogs.size() == 2
                    && mixed.cogs[0].values.size() == 1
                    && mixed.cogs[1].values.size() == 1
                    && mixed.cogs[0].values[0].type == sed::ValueType::str
                    && mixed.cogs[0].values[0].stringValue == "0"
                    && mixed.cogs[1].values[0].type == sed::ValueType::str
                    && mixed.cogs[1].values[0].stringValue == "0",
                "JKL.COGSurfaceOrder: mixed finalization failure is atomic");
        }

        sed::assets::finalizeJklImportResources(deferred, resources);
        check(checkSurfaceTarget(deferred),
            "JKL.COGSurfaceOrder: deferred resource finalization preserves wire target identity");

        std::ostringstream exported;
        sed::assets::saveToJKL(immediate, exported);
        check(exported.str().find("target.cog\t1") != std::string::npos,
            "JKL.COGSurfaceOrder: corrected editor target is written as the expected wire index");
        std::istringstream reimportInput(exported.str());
        const sed::Level reimported = sed::assets::loadFromJKL(reimportInput, immediateOptions);
        check(checkSurfaceTarget(reimported),
            "JKL.COGSurfaceOrder: corrected target survives JKL export and import");

        std::filesystem::remove_all(root, ec);
    }

    {
        // Real IJIM levels contain valid, very small faces whose every edge is
        // below the editor's preferred UV-basis length. Scale recovery must
        // still use the best non-degenerate triangle instead of exhausting
        // the candidate scan while looking for a larger edge.
        const char* ndy = R"NDY(
        SECTION: HEADER
        VERSION 3
        PIXELS PER UNIT 320
        END

        SECTION: MATERIALS
        WORLD MATERIALS 0
        END

        SECTION: GEORESOURCE
        WORLD VERTICES 3
        0: 0 0 0
        1: 0.0012 0 0
        2: 0 0.0012 0
        WORLD TEXTURE VERTICES 3
        0: 0 0
        1: 0.0012 0
        2: 0 0.0012
        WORLD ADJOINS 0
        WORLD SURFACES 1
        0: -1 0x0 0x0 0 3 3 -1 0 0 0 1 3 0,0 1,1 2,2 0 0 0 0 0 0 0 0 0
        0: 0 0 1
        END

        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0
        AMBIENT LIGHT 0 0 0
        EXTRA LIGHT 0 0 0
        SURFACES 0 1
        END
        )NDY";

        std::optional<sed::Level> level{};
        std::string error{};
        try
        {
            std::istringstream input(ndy);
            level = sed::assets::loadFromJKL(input);
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
        }

        const std::string loadMessage =
            "NDY: valid sub-centimetre surface loads and derives UV scale: "
            + error;
        check(level.has_value(), loadMessage.c_str());
        if (level && !level->sectors.empty()
            && !level->sectors[0].surfaces.empty())
        {
            const sed::Surface& surface = level->sectors[0].surfaces[0];
            check(close(surface.uScale, 1.0 / 320.0)
                    && close(surface.vScale, 1.0 / 320.0),
                "NDY: sub-centimetre normalized UV scale is reconstructed from the non-degenerate fallback triangle");
        }
    }

    {
        // Legacy NDY files can contain dormant zero-area surfaces with
        // repeated corners. The Pascal loader treated UV-scale recovery as
        // best effort for these records and retained the Surface defaults.
        // Rejecting the complete level here breaks otherwise usable projects.
        const char* ndy = R"NDY(
        SECTION: HEADER
        VERSION 3
        PIXELS PER UNIT 320
        END

        SECTION: MATERIALS
        WORLD MATERIALS 0
        END

        SECTION: GEORESOURCE
        WORLD VERTICES 2
        0: 0 0 0
        1: 0 0 1
        WORLD TEXTURE VERTICES 2
        0: 0 0
        1: 0 1
        WORLD ADJOINS 0
        WORLD SURFACES 1
        0: -1 0x0 0x0 0 3 3 -1 0 0 0 1 4 0,0 0,0 1,1 1,1 0 0 0 0 0 0 0 0 0 0 0 0
        0: 0 0 0
        END

        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0
        AMBIENT LIGHT 0 0 0
        EXTRA LIGHT 0 0 0
        SURFACES 0 1
        END
        )NDY";

        std::optional<sed::Level> level{};
        std::string error{};
        try
        {
            std::istringstream input(ndy);
            level = sed::assets::loadFromJKL(input);
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
        }

        const std::string loadMessage =
            "NDY: finite legacy degenerate surface retains default UV scale: "
            + error;
        check(level.has_value(), loadMessage.c_str());
        if (level && !level->sectors.empty()
            && !level->sectors[0].surfaces.empty())
        {
            const sed::Surface& surface = level->sectors[0].surfaces[0];
            check(close(surface.uScale, 1.0)
                    && close(surface.vScale, 1.0),
                "NDY: unrecoverable finite legacy UV basis keeps safe default scales");
        }
    }

    {
        // JKDF2/MotS use pixel-space UVs on disk, but scale recovery must use
        // the same small-face fallback as normalized NDY input before any
        // optional resource-backed normalization occurs.
        const char* jkl = R"JKL(
        SECTION: HEADER
        VERSION 1
        PIXELS PER UNIT 320
        END

        SECTION: MATERIALS
        WORLD MATERIALS 0
        END

        SECTION: GEORESOURCE
        WORLD COLORMAPS 0
        WORLD VERTICES 3
        0: 0 0 0
        1: 0.0012 0 0
        2: 0 0.0012 0
        WORLD TEXTURE VERTICES 3
        0: 0 0
        1: 0.384 0
        2: 0 0.384
        WORLD ADJOINS 0
        WORLD SURFACES 1
        0: -1 0x0 0x0 0 3 3 -1 0 3 0,0 1,1 2,2 1 1 1
        0: 0 0 1
        END

        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0
        AMBIENT LIGHT 0
        EXTRA LIGHT 0
        SURFACES 0 1
        END
        )JKL";

        std::istringstream input(jkl);
        const sed::Level level = sed::assets::loadFromJKL(input);

        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::TexturePixels,
            "JKL: small-face fixture remains explicitly pixel-space without resources");
        check(!level.sectors.empty()
                && !level.sectors[0].surfaces.empty()
                && close(level.sectors[0].surfaces[0].uScale, 1.0)
                && close(level.sectors[0].surfaces[0].vScale, 1.0),
            "JKL: valid sub-centimetre pixel-space surface recovers both authored scales");
    }

    {
        const char* jkl = R"JKL(
        SECTION: HEADER
        VERSION 1
        WORLD GRAVITY 9
        MIPMAP DISTANCES 11 22 33 44
        LOD DISTANCES 1.1 2.2 3.3 4.4
        PERSPECTIVE DISTANCE 5.5
        GOURAUD DISTANCE 6.6
        FOG TRUE 0.1 0.2 0.3 0.4 7.7 8.8
        CEILING SKY Z 123
        CEILING SKY OFFSET 1.5 2.5
        HORIZON DISTANCE 500
        HORIZON PIXELS PER REV 999
        HORIZON SKY OFFSET 7 8
        END

        SECTION: MATERIALS
        WORLD MATERIALS 1
        0: MAT: test.mat
        END

        SECTION: GEORESOURCE
        WORLD COLORMAPS 1
        0: test.cmp
        WORLD VERTICES 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        WORLD TEXTURE VERTICES 3
        0: 0 0
        1: 1 0
        2: 0 1
        WORLD ADJOINS 0
        WORLD SURFACES 1
        0: 0 0x1 0x20 1 2 3 -1 -0.25 3 0,0 1,1 2,2 0.1 0.2 0.3
        0: 0 0 1
        END

        SECTION: SECTORS
        WORLD SECTORS 1
        SECTOR 0
        FLAGS 0x123
        AMBIENT LIGHT 0.5
        EXTRA LIGHT 0.1
        AVERAGE LIGHT INTENSITY 0.11 0.22 0.33
        AVERAGE LIGHT POSITION 1 2 3
        AVERAGE LIGHT FALLOFF 4 5
        SOUND amb.wav 0.5
        COLORMAP 0
        COLLIDEBOX 1 2 3 4 5 6
        TINT 0.2 0.3 0.4
        THRUST 7 8 9
        SURFACES 0 1
        END

        SECTION: THINGS
        WORLD THINGS 1
        0: thing walkplayer 1 2 3 10 20 30 0 health=100
        END

        SECTION: LIGHTS
        EDITOR LIGHTS 1
        0: 0x1 0 1 2 3 4 5 0.25 0.5 0.75
        END

        SECTION: LAYERS
        EDITOR LAYERS 1
        Main
        1: 0
        1: 0
        lvis test
        END
        )JKL";

        std::istringstream iss(jkl);
        const sed::Level level = sed::assets::loadFromJKL(iss);
        check(level.kind == sed::ProjectType::JKDF2, "JKL: kind");
        check(level.header.version == 1, "JKL: version");
        check(close(level.header.gravity, 9.0), "JKL: gravity");
        check(level.header.fog.enabled, "JKL: fog enabled");
        check(close(level.header.fog.color.r, 0.1), "JKL: fog.r");
        check(close(level.header.fog.color.g, 0.2), "JKL: fog.g");
        check(close(level.header.fog.color.b, 0.3), "JKL: fog.b");
        check(close(level.header.fog.color.a, 0.4), "JKL: fog.a");
        check(close(level.header.fog.fogStart, 7.7), "JKL: fog.start");
        check(close(level.header.fog.fogEnd, 8.8), "JKL: fog.end");
        check(close(level.header.ceilingSky.height, 123.0), "JKL: ceilingSky.height");
        check(close(level.header.ceilingSky.offset.x, 1.5), "JKL: ceilingSky.offset.x");
        check(close(level.header.ceilingSky.offset.y, 2.5), "JKL: ceilingSky.offset.y");
        check(close(level.header.horizonSky.distance, 500.0), "JKL: horizonSky.distance");
        check(close(level.header.horizonSky.pixelsPerRev, 999.0), "JKL: horizonSky.pixelsPerRev");
        check(close(level.header.horizonSky.offset.x, 7.0), "JKL: horizonSky.offset.x");
        check(close(level.header.horizonSky.offset.y, 8.0), "JKL: horizonSky.offset.y");
        check(close(level.header.mipmapDistances[0], 11.0), "JKL: mipmapDistances[0]");
        check(close(level.header.mipmapDistances[1], 22.0), "JKL: mipmapDistances[1]");
        check(close(level.header.mipmapDistances[2], 33.0), "JKL: mipmapDistances[2]");
        check(close(level.header.mipmapDistances[3], 44.0), "JKL: mipmapDistances[3]");
        check(close(level.header.lodDistances[0], 1.1), "JKL: lodDistances[0]");
        check(close(level.header.lodDistances[1], 2.2), "JKL: lodDistances[1]");
        check(close(level.header.lodDistances[2], 3.3), "JKL: lodDistances[2]");
        check(close(level.header.lodDistances[3], 4.4), "JKL: lodDistances[3]");
        check(close(level.header.perspectiveDistance, 5.5), "JKL: perspectiveDistance");
        check(close(level.header.gouraudDistance, 6.6), "JKL: gouraudDistance");

        check(level.sectors.size() == 1, "JKL: sectors");
        check(level.sectors[0].surfaces.size() == 1, "JKL: surfaces");
        check(level.sectors[0].surfaces.size() == 1 && level.sectors[0].surfaces[0].num == 0, "JKL: surface.num");
        check(level.sectors[0].vertices.size() == 3, "JKL: vertices");
        check(level.materials.size() == 1, "JKL: materials");
        check(level.materials.size() == 1 && level.materials[0] == "test.mat", "JKL: material name");
        check(level.colormaps.size() == 1, "JKL: colormaps");
        check(level.colormaps.size() == 1 && level.colormaps[0] == "test.cmp", "JKL: colormap name");
        check(level.sectors[0].surfaces.size() == 1 && level.sectors[0].surfaces[0].material == "test.mat",
              "JKL: surface material name");

        check(level.layers.size() == 1, "JKL: layers");
        check(level.layers.size() == 1 && level.layers[0] == "Main", "JKL: layer name");
        check(level.lvisstring == "lvis test", "JKL: lvisstring");

        check(level.things.size() == 1, "JKL: things");
        check(level.lights.size() == 1, "JKL: lights");

        if (!level.sectors.empty())
        {
            const sed::Sector& sector = level.sectors[0];
            check(sector.flags == 0x123, "JKL: sector.flags");
            check(close(sector.ambient.r, 0.5), "JKL: sector.ambient.r");
            check(close(sector.ambient.g, 0.5), "JKL: sector.ambient.g");
            check(close(sector.ambient.b, 0.5), "JKL: sector.ambient.b");
            check(close(sector.ambient.a, 0.0), "JKL: sector.ambient.a");

            check(close(sector.extraLight.r, 0.1), "JKL: sector.extraLight.r");
            check(close(sector.extraLight.g, 0.1), "JKL: sector.extraLight.g");
            check(close(sector.extraLight.b, 0.1), "JKL: sector.extraLight.b");
            check(close(sector.extraLight.a, 0.0), "JKL: sector.extraLight.a");

            check(close(sector.pointLight.color.r, 0.11), "JKL: sector.pointLight.color.r");
            check(close(sector.pointLight.color.g, 0.22), "JKL: sector.pointLight.color.g");
            check(close(sector.pointLight.color.b, 0.33), "JKL: sector.pointLight.color.b");

            check(close(sector.pointLight.position.x, 1.0), "JKL: sector.pointLight.position.x");
            check(close(sector.pointLight.position.y, 2.0), "JKL: sector.pointLight.position.y");
            check(close(sector.pointLight.position.z, 3.0), "JKL: sector.pointLight.position.z");

            check(close(sector.pointLight.minRange, 4.0), "JKL: sector.pointLight.minRange");
            check(close(sector.pointLight.maxRange, 5.0), "JKL: sector.pointLight.maxRange");

            check(sector.sound == "amb.wav", "JKL: sector.sound");
            check(close(sector.soundVolume, 0.5), "JKL: sector.soundVolume");
            check(sector.colormap == "test.cmp", "JKL: sector.colormap");

            check(close(sector.collideBox.x1, 1.0), "JKL: sector.collideBox.x1");
            check(close(sector.collideBox.y1, 2.0), "JKL: sector.collideBox.y1");
            check(close(sector.collideBox.z1, 3.0), "JKL: sector.collideBox.z1");
            check(close(sector.collideBox.x2, 4.0), "JKL: sector.collideBox.x2");
            check(close(sector.collideBox.y2, 5.0), "JKL: sector.collideBox.y2");
            check(close(sector.collideBox.z2, 6.0), "JKL: sector.collideBox.z2");

            check(close(sector.tint.r, 0.2), "JKL: sector.tint.r");
            check(close(sector.tint.g, 0.3), "JKL: sector.tint.g");
            check(close(sector.tint.b, 0.4), "JKL: sector.tint.b");

            check(close(sector.thrust.x, 7.0), "JKL: sector.thrust.x");
            check(close(sector.thrust.y, 8.0), "JKL: sector.thrust.y");
            check(close(sector.thrust.z, 9.0), "JKL: sector.thrust.z");

            check(sector.vertices.size() == 3, "JKL: sector.vertices.size");
            if (sector.vertices.size() >= 3)
            {
                check(close(sector.vertices[0].x, 0.0), "JKL: sector.v0.x");
                check(close(sector.vertices[0].y, 0.0), "JKL: sector.v0.y");
                check(close(sector.vertices[0].z, 0.0), "JKL: sector.v0.z");
                check(sector.vertices[0].mark == 0, "JKL: sector.v0.mark");

                check(close(sector.vertices[1].x, 1.0), "JKL: sector.v1.x");
                check(close(sector.vertices[1].y, 0.0), "JKL: sector.v1.y");
                check(close(sector.vertices[1].z, 0.0), "JKL: sector.v1.z");
                check(sector.vertices[1].mark == 0, "JKL: sector.v1.mark");

                check(close(sector.vertices[2].x, 0.0), "JKL: sector.v2.x");
                check(close(sector.vertices[2].y, 1.0), "JKL: sector.v2.y");
                check(close(sector.vertices[2].z, 0.0), "JKL: sector.v2.z");
                check(sector.vertices[2].mark == 0, "JKL: sector.v2.mark");
            }

            check(sector.surfaces.size() == 1, "JKL: sector.surfaces.size");
            if (!sector.surfaces.empty())
            {
                const sed::Surface& surf = sector.surfaces[0];
                check(surf.nMat == 0, "JKL: surface.nMat");
                check(surf.surfaceFlags == 0x1, "JKL: surface.surfaceFlags");
                check(surf.faceFlags == 0x20u, "JKL: surface.faceFlags");
                check(surf.geo == 1, "JKL: surface.geo");
                check(surf.light == 2, "JKL: surface.light");
                check(surf.tex == 3, "JKL: surface.tex");
                check(surf.nAdj == -1, "JKL: surface.nAdj");
                check(!surf.adjoin.has_value(), "JKL: surface.adjoin");
                check(surf.adjoinFlags == 0, "JKL: surface.adjoinFlags");
                check(close(surf.uScale, 1.0 / 320.0), "JKL: surface.uScale");
                check(close(surf.vScale, 1.0 / 320.0), "JKL: surface.vScale");

                check(close(surf.extraLight.r, -0.25), "JKL: surface.extraLight.r");
                check(close(surf.extraLight.g, -0.25), "JKL: surface.extraLight.g");
                check(close(surf.extraLight.b, -0.25), "JKL: surface.extraLight.b");
                check(close(surf.extraLight.a, 1.0), "JKL: surface.extraLight.a");

                check(surf.vertices.size() == 3, "JKL: surface.vertices.size");
                check(surf.vertices.size() >= 3 && surf.vertices[0] == 0 && surf.vertices[1] == 1 && surf.vertices[2] == 2,
                      "JKL: surface.vertices");
                check(surf.texVertices.size() == 3, "JKL: surface.texVertices.size");
                if (surf.texVertices.size() >= 3)
                {
                    check(close(surf.texVertices[0].u, 0.0), "JKL: surface.tv0.u");
                    check(close(surf.texVertices[0].v, 0.0), "JKL: surface.tv0.v");
                    check(close(surf.texVertices[0].color.r, 0.1), "JKL: surface.tv0.color.r");
                    check(close(surf.texVertices[0].color.g, 0.1), "JKL: surface.tv0.color.g");
                    check(close(surf.texVertices[0].color.b, 0.1), "JKL: surface.tv0.color.b");
                    check(close(surf.texVertices[0].color.a, 0.1), "JKL: surface.tv0.color.a");

                    check(close(surf.texVertices[1].u, 1.0), "JKL: surface.tv1.u");
                    check(close(surf.texVertices[1].v, 0.0), "JKL: surface.tv1.v");
                    check(close(surf.texVertices[1].color.r, 0.2), "JKL: surface.tv1.color.r");
                    check(close(surf.texVertices[1].color.g, 0.2), "JKL: surface.tv1.color.g");
                    check(close(surf.texVertices[1].color.b, 0.2), "JKL: surface.tv1.color.b");
                    check(close(surf.texVertices[1].color.a, 0.2), "JKL: surface.tv1.color.a");

                    check(close(surf.texVertices[2].u, 0.0), "JKL: surface.tv2.u");
                    check(close(surf.texVertices[2].v, 1.0), "JKL: surface.tv2.v");
                    check(close(surf.texVertices[2].color.r, 0.3), "JKL: surface.tv2.color.r");
                    check(close(surf.texVertices[2].color.g, 0.3), "JKL: surface.tv2.color.g");
                    check(close(surf.texVertices[2].color.b, 0.3), "JKL: surface.tv2.color.b");
                    check(close(surf.texVertices[2].color.a, 0.3), "JKL: surface.tv2.color.a");
                }

                check(closeVec(surf.normal, sed::TVector{ 0.0, 0.0, 1.0 }, kRotationEpsilon), "JKL: surface.normal");
            }
        }

        if (!level.things.empty())
        {
            const sed::Thing& thing = level.things[0];
            check(thing.name == "walkplayer", "JKL: thing.name");
            check(close(thing.x, 1.0), "JKL: thing.x");
            check(close(thing.y, 2.0), "JKL: thing.y");
            check(close(thing.z, 3.0), "JKL: thing.z");
            check(close(thing.pch, 10.0), "JKL: thing.pch");
            check(close(thing.yaw, 20.0), "JKL: thing.yaw");
            check(close(thing.rol, 30.0), "JKL: thing.rol");
            check(thing.sector == 0, "JKL: thing.sector");
            check(thing.layer == 0, "JKL: thing.layer");

            check(thing.values.size() == 1, "JKL: thing.values.size");
            check(thing.values.size() == 1 && thing.values[0].name == "health", "JKL: thing.values[0].name");
            check(thing.values.size() == 1 && thing.values[0].stringValue == "100", "JKL: thing.values[0].value");
        }

        if (!level.lights.empty())
        {
            const sed::SedLight& light = level.lights[0];
            check(light.flags == 0x1, "JKL: light.flags");
            check(light.layer == 0, "JKL: light.layer");
            check(close(light.position.x, 1.0), "JKL: light.position.x");
            check(close(light.position.y, 2.0), "JKL: light.position.y");
            check(close(light.position.z, 3.0), "JKL: light.position.z");
            check(close(light.range, 4.0), "JKL: light.range");
            check(close(light.intensity, 5.0), "JKL: light.intensity");
            check(close(light.color.r, 0.25), "JKL: light.color.r");
            check(close(light.color.g, 0.5), "JKL: light.color.g");
            check(close(light.color.b, 0.75), "JKL: light.color.b");
        }
    }

    {
        auto makeMultiSurfaceJkl = [](const std::int32_t surfaceCount) -> std::string
        {
            std::ostringstream out;
            out
                << "SECTION: HEADER\n"
                << "VERSION 1\n"
                << "END\n\n"
                << "SECTION: MATERIALS\n"
                << "WORLD MATERIALS 1\n"
                << "0: MAT: test.mat\n"
                << "END\n\n"
                << "SECTION: GEORESOURCE\n"
                << "WORLD COLORMAPS 0\n"
                << "WORLD VERTICES 3\n"
                << "0: 0 0 0\n"
                << "1: 1 0 0\n"
                << "2: 0 1 0\n"
                << "WORLD TEXTURE VERTICES 3\n"
                << "0: 0 0\n"
                << "1: 1 0\n"
                << "2: 0 1\n"
                << "WORLD ADJOINS 0\n"
                << "WORLD SURFACES " << surfaceCount << "\n";

            for (std::int32_t i = 0; i < surfaceCount; ++i)
            {
                out
                    << i << ": 0 0x0 0x0 0 0 0 -1 0.0 3 0,0 1,1 2,2 1.0 1.0 1.0\n"
                    << "0: 0 0 1\n";
            }

            out
                << "END\n\n"
                << "SECTION: SECTORS\n"
                << "WORLD SECTORS 1\n"
                << "SECTOR 0\n"
                << "FLAGS 0x0\n"
                << "AMBIENT LIGHT 0.5\n"
                << "SURFACES 0 " << surfaceCount << "\n"
                << "END\n\n"
                << "SECTION: THINGS\n"
                << "WORLD THINGS 0\n"
                << "END\n\n"
                << "SECTION: LIGHTS\n"
                << "EDITOR LIGHTS 0\n"
                << "END\n\n"
                << "SECTION: LAYERS\n"
                << "EDITOR LAYERS 0\n"
                << "END\n";

            return out.str();
        };

        for (const std::int32_t surfaceCount : { 3, 4, 5 })
        {
            std::istringstream iss(makeMultiSurfaceJkl(surfaceCount));
            const sed::Level level = sed::assets::loadFromJKL(iss);

            check(level.sectors.size() == 1, "JKL.multiSurface: sectors");
            if (level.sectors.size() != 1)
            {
                continue;
            }

            const sed::Sector& sector = level.sectors[0];
            check(static_cast<std::int32_t>(sector.surfaces.size()) == surfaceCount, "JKL.multiSurface: surfaces");
            if (static_cast<std::int32_t>(sector.surfaces.size()) != surfaceCount)
            {
                continue;
            }

            for (std::int32_t i = 0; i < surfaceCount; ++i)
            {
                check(sector.surfaces[static_cast<std::size_t>(i)].num == i, "JKL.multiSurface: surface.num");
            }
        }
    }

    {
        const char* jkl = R"JKL(
        section: header
        version 1
        world gravity 4
        end

        section: materials
        world materials 1
        0: mat: test.mat
        end

        section: georesource
        world colormaps 0
        world vertices 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        world texture vertices 3
        0: 0 0
        1: 1 0
        2: 0 1
        world adjoins 0
        world surfaces 1
        0: 0 0x0 0x0 0 0 0 -1 0.0 3 0,0 1,1 2,2 1.0 1.0 1.0
        0: 0 0 1
        end

        section: sectors
        world sectors 1
        sector 0
        flags 0x0
        ambient light 0.5
        surfaces 0 1
        end

        section: things
        world things 0
        end

        section: lights
        editor lights 0
        end

        section: layers
        editor layers 0
        end
        )JKL";

        std::istringstream iss(jkl);
        const sed::Level level = sed::assets::loadFromJKL(iss);
        check(level.sectors.size() == 1, "JKL.lowercase: sectors");
        check(level.sectors[0].surfaces.size() == 1, "JKL.lowercase: surfaces");
        check(level.sectors[0].vertices.size() == 3, "JKL.lowercase: vertices");
        check(level.materials.size() == 1, "JKL.lowercase: materials");
        check(level.materials.size() == 1 && level.materials[0] == "test.mat", "JKL.lowercase: material name");
        check(level.sectors[0].surfaces.size() == 1 && level.sectors[0].surfaces[0].material == "test.mat",
              "JKL.lowercase: surface material name");
    }

    {
        const char* jkl = R"JKL(
        SeCtIoN: HeAdEr
        VeRsIoN 1
        WoRlD GrAvItY 4
        EnD

        sEcTiOn: MaTeRiAlS
        wOrLd mAtErIaLs 1
        0: mAt: test.mat
        eNd

        SeCtIoN: GeOrEsOuRcE
        wOrLd CoLoRmApS 0
        WoRlD vErTiCeS 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        WoRlD tExTuRe VeRtIcEs 3
        0: 0 0
        1: 1 0
        2: 0 1
        wOrLd aDjOiNs 0
        WoRlD sUrFaCeS 1
        0: 0 0x0 0x0 0 0 0 -1 0.0 3 0,0 1,1 2,2 1.0 1.0 1.0
        0: 0 0 1
        eNd

        SeCtIoN: SeCtOrS
        WoRlD sEcToRs 1
        sEcToR 0
        fLaGs 0x0
        aMbIeNt lIgHt 0.5
        sUrFaCeS 0 1
        EnD

        SeCtIoN: ThInGs
        WoRlD tHiNgS 0
        eNd
        )JKL";

        std::istringstream iss(jkl);
        const sed::Level level = sed::assets::loadFromJKL(iss);
        check(level.sectors.size() == 1, "JKL.mixedcase: sectors");
        check(level.sectors[0].surfaces.size() == 1, "JKL.mixedcase: surfaces");
        check(level.sectors[0].vertices.size() == 3, "JKL.mixedcase: vertices");
        check(level.materials.size() == 1, "JKL.mixedcase: materials");
        check(level.materials.size() == 1 && level.materials[0] == "test.mat", "JKL.mixedcase: material name");
        check(level.sectors[0].surfaces.size() == 1 && level.sectors[0].surfaces[0].material == "test.mat",
              "JKL.mixedcase: surface material name");
    }

    {
        const char* ndy = R"NDY(
        section: header
        version 3
        end

        section: materials
        world materials 1
        0: mat: test.mat
        end

        section: georesource
        world vertices 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        world texture vertices 3
        0: 0 0
        1: 1 0
        2: 0 1
        world adjoins 0
        world surfaces 1
        0: 0 0x0 0x0 0 0 0 -1 0.1 0.2 0.3 1.0 3 0,0 1,1 2,2 1 0 0 0 1 0 0 0 1
        0: 0 0 1
        end

        section: sectors
        world sectors 1
        sector 0
        flags 0x0
        ambient light 0.25 0.25 0.25
        surfaces 0 1
        end

        section: things
        world things 0
        end
        )NDY";

        std::istringstream iss(ndy);
        const sed::Level level = sed::assets::loadFromJKL(iss);

        check(level.kind == sed::ProjectType::IJIM, "NDY: kind");
        check(level.header.version == 3, "NDY: version");
        check(close(level.header.gravity, 2.0), "NDY: default gravity");
        check(level.colormaps.empty(), "NDY: no global colormaps");

        check(level.materials.size() == 1, "NDY: materials");
        check(level.materials.size() == 1 && level.materials[0] == "test.mat", "NDY: material name");

        check(level.sectors.size() == 1, "NDY: sectors");
        check(level.sectors[0].surfaces.size() == 1, "NDY: surfaces");
        check(level.sectors[0].vertices.size() == 3, "NDY: vertices");
        check(level.sectors[0].surfaces[0].material == "test.mat", "NDY: surface material name");

        if (!level.sectors.empty())
        {
            const sed::Sector& sector = level.sectors[0];
            check(close(sector.ambient.r, 0.25), "NDY: sector.ambient.r");
            check(close(sector.ambient.g, 0.25), "NDY: sector.ambient.g");
            check(close(sector.ambient.b, 0.25), "NDY: sector.ambient.b");
            check(close(sector.ambient.a, 0.0), "NDY: sector.ambient.a");

            if (!sector.surfaces.empty())
            {
                const sed::Surface& surf = sector.surfaces[0];

                check(close(surf.extraLight.r, 0.1), "NDY: surface.extraLight.r");
                check(close(surf.extraLight.g, 0.2), "NDY: surface.extraLight.g");
                check(close(surf.extraLight.b, 0.3), "NDY: surface.extraLight.b");
                check(close(surf.extraLight.a, 1.0), "NDY: surface.extraLight.a");

                check(surf.vertices.size() == 3, "NDY: surface.vertices.size");
                check(surf.texVertices.size() == 3, "NDY: surface.texVertices.size");
                if (surf.texVertices.size() >= 3)
                {
                    check(close(surf.texVertices[0].u, 0.0), "NDY: surface.tv0.u");
                    check(close(surf.texVertices[0].v, 0.0), "NDY: surface.tv0.v");
                    check(close(surf.texVertices[0].color.r, 1.0), "NDY: surface.tv0.color.r");
                    check(close(surf.texVertices[0].color.g, 0.0), "NDY: surface.tv0.color.g");
                    check(close(surf.texVertices[0].color.b, 0.0), "NDY: surface.tv0.color.b");
                    check(close(surf.texVertices[0].color.a, 1.0), "NDY: surface.tv0.color.a");

                    check(close(surf.texVertices[1].u, 1.0), "NDY: surface.tv1.u");
                    check(close(surf.texVertices[1].v, 0.0), "NDY: surface.tv1.v");
                    check(close(surf.texVertices[1].color.r, 0.0), "NDY: surface.tv1.color.r");
                    check(close(surf.texVertices[1].color.g, 1.0), "NDY: surface.tv1.color.g");
                    check(close(surf.texVertices[1].color.b, 0.0), "NDY: surface.tv1.color.b");
                    check(close(surf.texVertices[1].color.a, 1.0), "NDY: surface.tv1.color.a");

                    check(close(surf.texVertices[2].u, 0.0), "NDY: surface.tv2.u");
                    check(close(surf.texVertices[2].v, 1.0), "NDY: surface.tv2.v");
                    check(close(surf.texVertices[2].color.r, 0.0), "NDY: surface.tv2.color.r");
                    check(close(surf.texVertices[2].color.g, 0.0), "NDY: surface.tv2.color.g");
                    check(close(surf.texVertices[2].color.b, 1.0), "NDY: surface.tv2.color.b");
                    check(close(surf.texVertices[2].color.a, 1.0), "NDY: surface.tv2.color.a");
                }

                check(closeVec(surf.normal, sed::TVector{ 0.0, 0.0, 1.0 }, kRotationEpsilon), "NDY: surface.normal");
            }
        }
    }

    {
        const char* ndy = R"NDY(
        section: header
        version 3
        end

        section: materials
        world materials 0
        end

        section: georesource
        world vertices 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        world texture vertices 3
        0: 0 0
        1: 1 0
        2: 0 1
        world adjoins 0
        world surfaces 1
        0: 32768 0x0 0x0 0 0 0 -1 0.1 0.2 0.3 1.0 3 0,0 1,1 2,2 1 0 0 0 1 0 0 0 1
        0: 0 0 1
        end

        section: sectors
        world sectors 1
        sector 0
        flags 0x0
        ambient light 0.25 0.25 0.25
        surfaces 0 1
        end

        section: things
        world things 0
        end
        )NDY";

        std::istringstream iss(ndy);
        const sed::Level level = sed::assets::loadFromJKL(iss);

        check(level.kind == sed::ProjectType::IJIM, "NDY.static_mat: kind");
        check(level.materials.empty(), "NDY.static_mat: materials.empty");
        check(level.sectors.size() == 1, "NDY.static_mat: sectors");

        if (!level.sectors.empty())
        {
            const sed::Sector& sector = level.sectors[0];
            check(sector.surfaces.size() == 1, "NDY.static_mat: surfaces");
            if (!sector.surfaces.empty())
            {
                const sed::Surface& surf = sector.surfaces[0];
                check(surf.nMat == 0x8000, "NDY.static_mat: surface.nMat");
                check(surf.material == "gen_4indy_shadow.mat", "NDY.static_mat: surface.material");
            }
        }
    }

    {
        const char* ndy = R"NDY(
        SeCtIoN: HeAdEr
        VeRsIoN 3
        EnD

        sEcTiOn: MaTeRiAlS
        WoRlD MaTeRiAlS 1
        0: MaT: test.mat
        EnD

        SeCtIoN: GeOrEsOuRcE
        WoRlD VeRtIcEs 3
        0: 0 0 0
        1: 1 0 0
        2: 0 1 0
        WoRlD TeXtUrE VeRtIcEs 3
        0: 0 0
        1: 1 0
        2: 0 1
        WoRlD AdJoInS 0
        WoRlD SuRfAcEs 1
        0: 0 0x0 0x0 0 0 0 -1 0.1 0.2 0.3 1.0 3 0,0 1,1 2,2 1 0 0 0 1 0 0 0 1
        0: 0 0 1
        EnD

        SeCtIoN: SeCtOrS
        WoRlD SeCtOrS 1
        SeCtOr 0
        FlAgS 0x0
        AmBiEnT LiGhT 0.25 0.25 0.25
        SuRfAcEs 0 1
        EnD

        SeCtIoN: ThInGs
        WoRlD ThInGs 0
        EnD
        )NDY";

        std::istringstream iss(ndy);
        const sed::Level level = sed::assets::loadFromJKL(iss);

        check(level.kind == sed::ProjectType::IJIM, "NDY.mixedcase: kind");
        check(level.header.version == 3, "NDY.mixedcase: version");
        check(level.materials.size() == 1, "NDY.mixedcase: materials");
        check(level.materials.size() == 1 && level.materials[0] == "test.mat", "NDY.mixedcase: material name");
        check(level.sectors.size() == 1, "NDY.mixedcase: sectors");
        check(level.sectors[0].surfaces.size() == 1, "NDY.mixedcase: surfaces");
        check(level.sectors[0].vertices.size() == 3, "NDY.mixedcase: vertices");
        check(level.sectors[0].surfaces[0].material == "test.mat", "NDY.mixedcase: surface material name");
    }

    {
        const char* jkl =
            " \n"
            "        SECTION: TEMPLATES \n"
            "        WORLD TEMPLATES 2 \n"
            "        base none thingflags=0x10 frame=(1/2/3:4/5/6) \n"
            "        child base creatething=base \n"
            "        END \n"
            "        ";

        std::istringstream iss(jkl);
        const sed::Level level = sed::assets::loadFromJKL(iss);

        check(level.templates.size() == 2, "JKL: templates.size");
        if (level.templates.size() >= 2)
        {
            const sed::ThingTemplate& base = level.templates[0];
            check(base.name == "base", "JKL: templates[0].name");
            check(base.parent == "none", "JKL: templates[0].parent");
            check(base.values.size() == 2, "JKL: templates[0].values.size");
            if (base.values.size() >= 2)
            {
                check(base.values[0].name == "thingflags", "JKL: templates[0].values[0].name");
                check(base.values[0].type == sed::ValueType::flag, "JKL: templates[0].values[0].type");
                check(base.values[0].intValue == 0x10, "JKL: templates[0].values[0].intValue");

                check(base.values[1].name == "frame", "JKL: templates[0].values[1].name");
                check(base.values[1].type == sed::ValueType::frame, "JKL: templates[0].values[1].type");
                check(base.values[1].stringValue == "(1/2/3:4/5/6)", "JKL: templates[0].values[1].stringValue");
            }

            const sed::ThingTemplate& child = level.templates[1];
            check(child.name == "child", "JKL: templates[1].name");
            check(child.parent == "base", "JKL: templates[1].parent");
            check(child.values.size() == 1, "JKL: templates[1].values.size");
            if (!child.values.empty())
            {
                check(child.values[0].name == "creatething", "JKL: templates[1].values[0].name");
                check(child.values[0].adjType == sed::AdjType::tpl, "JKL: templates[1].values[0].adjType");
                check(child.values[0].stringValue == "base", "JKL: templates[1].values[0].stringValue");
            }
        }
    }

    {
        // Frame values must be parseable (3 or 6 components) and always stored as a single token (no whitespace).
        {
            sed::TplValue v{};
            v.name = "frame";
            v.type = sed::ValueType::frame;
            check(v.parse("(1/2/3:4/5/6)"), "FrameValue.parse: canonical");

            double x{}, y{}, z{}, pch{}, yaw{}, rol{};
            v.getFrame(x, y, z, pch, yaw, rol);
            check(x == 1.0 && y == 2.0 && z == 3.0 && pch == 4.0 && yaw == 5.0 && rol == 6.0, "FrameValue.getFrame: canonical");

            double x2{}, y2{}, z2{}, p2{}, ya2{}, r2{};
            check(v.tryGetFrame(x2, y2, z2, p2, ya2, r2), "FrameValue.tryGetFrame: canonical");
            check(x2 == 1.0 && y2 == 2.0 && z2 == 3.0 && p2 == 4.0 && ya2 == 5.0 && r2 == 6.0, "FrameValue.tryGetFrame.values: canonical");
        }

        {
            sed::TplValue v{};
            v.name = "frame";
            v.type = sed::ValueType::frame;
            check(v.parse("(1/2/3)"), "FrameValue.parse: pos only");

            double x{}, y{}, z{}, pch{}, yaw{}, rol{};
            v.getFrame(x, y, z, pch, yaw, rol);
            check(x == 1.0 && y == 2.0 && z == 3.0 && pch == 0.0 && yaw == 0.0 && rol == 0.0, "FrameValue.getFrame: pos only");
        }

        {
            // tryGetFrame should reject invalid strings (no silent 0,0,0 fallback).
            sed::TplValue v{};
            v.name = "frame";
            v.type = sed::ValueType::frame;
            v.stringValue = "not_a_frame";

            double x{}, y{}, z{}, pch{}, yaw{}, rol{};
            check(!v.tryGetFrame(x, y, z, pch, yaw, rol), "FrameValue.tryGetFrame: invalid");

            // getFrame keeps legacy behavior (zeros on failure).
            v.getFrame(x, y, z, pch, yaw, rol);
            check(x == 0.0 && y == 0.0 && z == 0.0 && pch == 0.0 && yaw == 0.0 && rol == 0.0, "FrameValue.getFrame: invalid zeros");
        }

        {
            sed::TplValue v{};
            v.name = "frame";
            v.setFrame(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
            check(v.type == sed::ValueType::frame, "FrameValue.setFrame: type");
            check(v.stringValue.find_first_of(" \t\r\n") == std::string::npos, "FrameValue.setFrame: whitespace-free");

            double x{}, y{}, z{}, pch{}, yaw{}, rol{};
            v.getFrame(x, y, z, pch, yaw, rol);
            check(x == 1.0 && y == 2.0 && z == 3.0 && pch == 4.0 && yaw == 5.0 && rol == 6.0, "FrameValue.roundtrip: set/get");
        }

        {
            sed::TplValue v{};
            v.name = "frame";
            v.type = sed::ValueType::frame;
            check(v.parse("1 2 3 4 5 6"), "FrameValue.parse: legacy whitespace");
            check(v.stringValue.find_first_of(" \t\r\n") == std::string::npos, "FrameValue.parse: legacy rewritten");

            double x{}, y{}, z{}, pch{}, yaw{}, rol{};
            v.getFrame(x, y, z, pch, yaw, rol);
            check(x == 1.0 && y == 2.0 && z == 3.0 && pch == 4.0 && yaw == 5.0 && rol == 6.0, "FrameValue.getFrame: legacy");
        }
    }

    {
        // `numframes` should track the number of `frame` values on a thing.
        {
            sed::Thing thing{};

            sed::TplValue f0{};
            f0.name = "frame";
            f0.setFrame(1.0, 2.0, 3.0, 0.0, 0.0, 0.0);
            thing.values.push_back(f0);

            sed::TplValue f1{};
            f1.name = "frame";
            f1.setFrame(4.0, 5.0, 6.0, 0.0, 0.0, 0.0);
            thing.values.push_back(f1);

            sed::TplValue num{};
            num.name = "numframes";
            num.type = sed::ValueType::integer;
            num.intValue = 99;
            thing.values.push_back(num);

            check(sed::level::thingCountFrames(thing) == 2, "thingCountFrames: count");
            sed::level::thingSyncNumFrames(thing);

            const sed::TplValue* got = nullptr;
            for (const sed::TplValue& v : thing.values)
            {
                if (v.name == "numframes")
                {
                    got = &v;
                    break;
                }
            }
            check(got != nullptr, "thingSyncNumFrames: has numframes");
            check(!thing.values.empty() && thing.values.front().name == "numframes",
                "thingSyncNumFrames: moves numframes before every frame");
            if (got)
            {
                check(got->type == sed::ValueType::integer, "thingSyncNumFrames: type integer");
                check(got->intValue == 2, "thingSyncNumFrames: intValue updated");
            }
        }

        {
            sed::Thing thing{};
            sed::TplValue f{};
            f.name = "frame";
            f.setFrame(1.0, 2.0, 3.0, 0.0, 0.0, 0.0);
            thing.values.push_back(f);

            check(sed::level::thingCountFrames(thing) == 1, "thingCountFrames: single");
            sed::level::thingSyncNumFrames(thing);

            bool ok = false;
            for (const sed::TplValue& v : thing.values)
            {
                if (v.name == "numframes")
                {
                    ok = (v.type == sed::ValueType::integer) && (v.intValue == 1);
                    break;
                }
            }
            check(ok, "thingSyncNumFrames: adds numframes");
            check(!thing.values.empty() && thing.values.front().name == "numframes",
                "thingSyncNumFrames: inserts numframes before the first frame");
        }

        {
            sed::Thing thing{};
            sed::TplValue num{};
            num.name = "numframes";
            num.type = sed::ValueType::integer;
            num.intValue = 5;
            thing.values.push_back(num);

            check(sed::level::thingCountFrames(thing) == 0, "thingCountFrames: none");
            sed::level::thingSyncNumFrames(thing);

            const bool hasNumframes = std::any_of(thing.values.begin(), thing.values.end(), [](const sed::TplValue& v)
                {
                    return v.name == "numframes";
                });
            check(!hasNumframes, "thingSyncNumFrames: removes numframes when no frames");
        }

        {
            sed::ThingTemplate thingTemplate{};
            sed::TplValue frame{};
            frame.name = "frame";
            frame.setFrame(1.0, 2.0, 3.0, 0.0, 0.0, 0.0);
            thingTemplate.values.push_back(frame);
            sed::TplValue misplacedCount{};
            misplacedCount.name = "numframes";
            misplacedCount.type = sed::ValueType::integer;
            misplacedCount.intValue = 99;
            thingTemplate.values.push_back(misplacedCount);

            sed::level::thingSyncNumFrames(thingTemplate);
            check(thingTemplate.values.size() == 2
                    && thingTemplate.values[0].name == "numframes"
                    && thingTemplate.values[0].intValue == 1
                    && thingTemplate.values[1].name == "frame",
                "thingSyncNumFrames: template values serialize allocate-before-append order");
        }
    }

    {
        const char* jkl =
            " \n"
            "        SECTION: COGS \n"
            "        WORLD COGS 1 \n"
            "        0: test.cog 123 4.5 (1/2/3)\n"
            "        END\n"
            "        ";

        std::istringstream iss(jkl);
        const sed::Level level = sed::assets::loadFromJKL(iss);

        check(level.cogs.size() == 1, "JKL: cogs.size");
        if (!level.cogs.empty())
        {
            const sed::Cog& cog = level.cogs[0];
            check(cog.name == "test.cog", "JKL: cogs[0].name");
            check(cog.values.size() == 3, "JKL: cogs[0].values.size");
            if (cog.values.size() >= 3)
            {
                check(cog.values[0].name == "param0", "JKL: cogs[0].values[0].name");
                check(cog.values[0].type == sed::ValueType::str, "JKL: cogs[0].values[0].type");
                check(cog.values[0].stringValue == "123", "JKL: cogs[0].values[0].stringValue");

                check(cog.values[1].name == "param1", "JKL: cogs[0].values[1].name");
                check(cog.values[1].stringValue == "4.5", "JKL: cogs[0].values[1].stringValue");

                check(cog.values[2].name == "param2", "JKL: cogs[0].values[2].name");
                check(cog.values[2].stringValue == "(1/2/3)", "JKL: cogs[0].values[2].stringValue");
            }
        }
    }

    {
        // COG values are emitted as one engine token per symbol.  Empty
        // resource defaults cannot be written as a blank tab field because
        // the following symbol would then be parsed into the wrong slot;
        // vectors likewise need one canonical parenthesized token even when
        // their authored spelling used commas or whitespace.
        sed::Level level{};
        level.header.version = 1;
        sed::Cog& cog = level.newCog();
        cog.name = "mixed_defaults.cog";

        sed::CogValue vector{};
        vector.name = "direction";
        vector.cogType = sed::CogType::vect;
        vector.type = sed::ValueType::vect;
        vector.stringValue = "1, 2, 3";

        sed::CogValue emptyVector = vector;
        emptyVector.name = "zero";
        emptyVector.stringValue.clear();

        sed::CogValue sound{};
        sound.name = "sound";
        sound.cogType = sed::CogType::wav;
        sound.type = sed::ValueType::str;

        sed::CogValue scalar{};
        scalar.name = "count";
        scalar.cogType = sed::CogType::int_;
        scalar.type = sed::ValueType::integer;
        scalar.intValue = 7;

        sed::CogValue local = scalar;
        local.name = "implementation_detail";
        local.local = true;

        sed::CogValue message{};
        message.name = "implementation_message";
        message.cogType = sed::CogType::msg;
        message.type = sed::ValueType::str;
        message.stringValue = "ignored";

        cog.values = { local, message, vector, emptyVector, sound, scalar };

        std::ostringstream exported;
        sed::assets::saveToJKL(level, exported);
        const std::string row = exported.str();
        check(row.find("0:\tmixed_defaults.cog\t(1/2/3)\t(0/0/0)\t-1\t7\n")
                != std::string::npos,
            "JKL.COGExport.defaults: mixed vector/resource/scalar values stay positional");
        check(row.find("WORLD SOUNDS 0\n") != std::string::npos,
            "JKL.COGExport.defaults: unset resource sentinel is not added to the sound list");

        const auto roundTripUnique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path roundTripRoot =
            "tmp_jkl_cog_default_roundtrip_" + roundTripUnique;
        std::error_code roundTripError;
        std::filesystem::create_directories(roundTripRoot / "cog", roundTripError);
        check(!roundTripError,
            "JKL.COGExport.defaults: round-trip resource directory creates");
        if (!roundTripError)
        {
            std::ofstream script(roundTripRoot / "cog" / "mixed_defaults.cog");
            script << "symbols\n"
                << "    vector direction=(0/0/0)\n"
                << "    vector zero=(0/0/0)\n"
                << "    wav sound=-1\n"
                << "    int count=0\n"
                << "end\n";
            script.close();

            sed::assets::ResourceLocator roundTripResources{};
            roundTripResources.addRoot(roundTripRoot);
            sed::assets::JklLoadOptions roundTripOptions{};
            roundTripOptions.resources = &roundTripResources;
            std::istringstream roundTripInput(row);
            const sed::Level hydrated = sed::assets::loadFromJKL(
                roundTripInput, roundTripOptions);
            check(hydrated.cogs.size() == 1
                    && hydrated.cogs[0].values.size() == 4
                    && hydrated.cogs[0].values[2].type == sed::ValueType::str
                    && hydrated.cogs[0].values[2].stringValue == "-1",
                "JKL.COGExport.defaults: resource hydration preserves an unset sound sentinel");
            std::ostringstream hydratedExport;
            sed::assets::saveToJKL(hydrated, hydratedExport);
            check(hydratedExport.str().find("WORLD SOUNDS 0\n") != std::string::npos,
                "JKL.COGExport.defaults: hydrated unset sound is excluded on re-export");
            std::filesystem::remove_all(roundTripRoot, roundTripError);
        }

        sed::CogValue invalidSound = sound;
        invalidSound.stringValue = "door open.wav";
        cog.values[2] = invalidSound;
        bool rejectedWhitespaceResource = false;
        try
        {
            std::ostringstream rejected;
            sed::assets::saveToJKL(level, rejected);
        }
        catch (const std::runtime_error& exception)
        {
            rejectedWhitespaceResource = std::string(exception.what())
                == "Cannot export JKL/NDY: COG 'mixed_defaults.cog' value 2 resource 'wav' contains whitespace and cannot be represented as one engine token.";
        }
        check(rejectedWhitespaceResource,
            "JKL.COGExport.defaults: whitespace resource names are rejected clearly");

        invalidSound.stringValue = "door#open.wav";
        cog.values[2] = invalidSound;
        bool rejectedCommentDelimiter = false;
        try
        {
            std::ostringstream rejected;
            sed::assets::saveToJKL(level, rejected);
        }
        catch (const std::runtime_error& exception)
        {
            rejectedCommentDelimiter = std::string(exception.what())
                == "Cannot export JKL/NDY: COG 'mixed_defaults.cog' value 2 contains '#' and cannot be represented as one engine token.";
        }
        check(rejectedCommentDelimiter,
            "JKL.COGExport.defaults: comment delimiter in a string is rejected clearly");

        sed::CogValue nonFinite{};
        nonFinite.name = "bad_float";
        nonFinite.cogType = sed::CogType::float_;
        nonFinite.type = sed::ValueType::floating;
        nonFinite.floatValue = std::numeric_limits<double>::quiet_NaN();
        cog.values = { nonFinite };
        bool rejectedNonFinite = false;
        try
        {
            std::ostringstream rejected;
            sed::assets::saveToJKL(level, rejected);
        }
        catch (const std::runtime_error& exception)
        {
            rejectedNonFinite = std::string(exception.what())
                == "Cannot export JKL/NDY: COG 'mixed_defaults.cog' value 0 floating value is not finite.";
        }
        check(rejectedNonFinite,
            "JKL.COGExport.defaults: nonfinite numeric values are rejected before writing");
    }

    {
        // Engine COG rows may omit trailing initializers.  Resource-backed
        // import must materialize the script defaults for both an empty row
        // and a partial row, while an unavailable script keeps every raw
        // token untouched.
        const auto unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root = "tmp_jkl_cog_omitted_defaults_" + unique;
        std::error_code ec;
        std::filesystem::create_directories(root / "cog", ec);
        check(!ec, "JKL.COGImport.defaults: resource directory creates");
        if (!ec)
        {
            std::ofstream script(root / "cog" / "defaults.cog");
            script << "symbols\n"
                << "    int count=0\n"
                << "    sector target\n"
                << "    vector direction\n"
                << "    wav sound\n"
                << "end\n";
            script.close();

            const char* jkl = R"JKL(SECTION: HEADER
VERSION 1
END
SECTION: GEORESOURCE
WORLD COLORMAPS 0
WORLD VERTICES 0
WORLD TEXTURE VERTICES 0
WORLD ADJOINS 0
WORLD SURFACES 0
END
SECTION: SECTORS
WORLD SECTORS 0
END
SECTION: COGS
WORLD COGS 3
0: defaults.cog
1: defaults.cog 7
2: missing.cog 1 2
END
)JKL";
            sed::assets::ResourceLocator resources{};
            resources.addRoot(root);
            sed::assets::JklLoadOptions options{};
            options.resources = &resources;
            std::istringstream input(jkl);
            const sed::Level level = sed::assets::loadFromJKL(input, options);
            check(level.cogs.size() == 3,
                "JKL.COGImport.defaults: all COG rows load");
            if (level.cogs.size() == 3)
            {
                const sed::Cog& empty = level.cogs[0];
                check(empty.values.size() == 4
                        && empty.values[0].type == sed::ValueType::integer
                        && empty.values[0].intValue == 0
                        && empty.values[1].type == sed::ValueType::ptr
                        && empty.values[1].cogType == sed::CogType::sec
                        && empty.values[1].intValue == -1
                        && empty.values[2].type == sed::ValueType::vect
                        && empty.values[2].stringValue == "(0/0/0)"
                        && empty.values[3].type == sed::ValueType::str
                        && empty.values[3].stringValue.empty(),
                    "JKL.COGImport.defaults: an empty row receives every typed default");

                const sed::Cog& partial = level.cogs[1];
                check(partial.values.size() == 4
                        && partial.values[0].type == sed::ValueType::integer
                        && partial.values[0].intValue == 7
                        && partial.values[1].type == sed::ValueType::ptr
                        && partial.values[1].cogType == sed::CogType::sec
                        && partial.values[1].intValue == -1
                        && partial.values[2].stringValue == "(0/0/0)"
                        && partial.values[3].stringValue.empty(),
                    "JKL.COGImport.defaults: a partial row preserves input and appends trailing defaults");

                const sed::Cog& unknown = level.cogs[2];
                check(unknown.values.size() == 2
                        && unknown.values[0].type == sed::ValueType::str
                        && unknown.values[0].stringValue == "1"
                        && unknown.values[1].type == sed::ValueType::str
                        && unknown.values[1].stringValue == "2",
                    "JKL.COGImport.defaults: an unavailable script preserves raw values");
            }
            std::filesystem::remove_all(root, ec);
        }
    }

    {
        // A native COG surface value may retain its preferred JED pair text
        // while its typed global index is unset. JKL uses one global surface
        // token, so resolve the pair instead of exporting -1.
        sed::Level level{};
        level.header.version = 1;
        sed::Sector& sector = level.newSector();
        sector.vertices = { sed::Vertex{0.0, 0.0, 0.0},
            sed::Vertex{1.0, 0.0, 0.0}, sed::Vertex{0.0, 1.0, 0.0} };
        sed::Surface surface{};
        surface.num = 0;
        surface.vertices = { 0, 1, 2 };
        surface.texVertices.resize(3);
        sector.surfaces.push_back(surface);

        sed::Cog& cog = level.newCog();
        cog.name = "surface_pair.cog";
        sed::CogValue target{};
        target.name = "target";
        target.cogType = sed::CogType::surface;
        target.type = sed::ValueType::ptr;
        target.intValue = -1;
        target.stringValue = "0 0";
        cog.values.push_back(target);

        std::ostringstream exported;
        sed::assets::saveToJKL(level, exported);
        check(exported.str().find("0:\tsurface_pair.cog\t0\n")
                != std::string::npos,
            "JKL.COGExport.surfacePair: valid preferred pair resolves to global wire index");

        cog.values[0].intValue = 0;
        cog.values[0].stringValue = "99 99";
        std::ostringstream stalePairFallback;
        sed::assets::saveToJKL(level, stalePairFallback);
        check(stalePairFallback.str().find("0:\tsurface_pair.cog\t0\n")
                != std::string::npos,
            "JKL.COGExport.surfacePair: valid typed target survives stale preferred pair text");

        cog.values[0].intValue = -1;
        bool rejectedInvalidPair = false;
        try
        {
            std::ostringstream rejected;
            sed::assets::saveToJKL(level, rejected);
        }
        catch (const std::runtime_error& exception)
        {
            rejectedInvalidPair = std::string(exception.what())
                == "Cannot export JKL/NDY: COG 'surface_pair.cog' value 0 has an invalid surface pair '99 99'.";
        }
        check(rejectedInvalidPair,
            "JKL.COGExport.surfacePair: invalid preferred pair is rejected clearly");
    }

    {
        const auto unique                = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path root = "tmp_jkl_cog_root_" + unique;
        const std::filesystem::path file = root / "cog" / "test.cog";

        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        check(!ec, "JKL.COG.ResourceLocator.create_directories");

        {
            std::ofstream out(file);
            out << "symbols\n";
            out << "    int foo=0\n";
            out << "    float bar=0\n";
            out << "    vector baz=(0/0/0)\n";
            out << "    sector sec=0\n";
            out << "    surface surf=0\n";
            out << "    thing thg=0\n";
            out << "    cog other=0\n";
            out << "    message msg=0\n";
            out << "    int localVar=0 local\n";
            out << "end\n";
        }

        const char* jkl = R"JKL(
        SECTION: COGS
        WORLD COGS 1
        0: test.cog 42 1.5 (1/2/3) 2 3 4 7
        END
        )JKL";

        sed::assets::ResourceLocator locator{};
        locator.addRoot(root);

        sed::assets::JklLoadOptions opts{};
        opts.resources = &locator;

        std::istringstream iss(jkl);
        const sed::Level level = sed::assets::loadFromJKL(iss, opts);

        check(level.cogs.size() == 1, "JKL.COG.ResourceLocator: cogs.size");
        if (!level.cogs.empty())
        {
            const sed::Cog& cog = level.cogs[0];
            check(cog.name == "test.cog", "JKL.COG.ResourceLocator: cogs[0].name");
            check(cog.values.size() == 7, "JKL.COG.ResourceLocator: cogs[0].values.size");
            if (cog.values.size() >= 7)
            {
                check(cog.values[0].name == "foo", "JKL.COG.ResourceLocator: foo.name");
                check(cog.values[0].type == sed::ValueType::integer, "JKL.COG.ResourceLocator: foo.type");
                check(cog.values[0].intValue == 42, "JKL.COG.ResourceLocator: foo.intValue");

                check(cog.values[1].name == "bar", "JKL.COG.ResourceLocator: bar.name");
                check(cog.values[1].type == sed::ValueType::floating, "JKL.COG.ResourceLocator: bar.type");
                check(close(cog.values[1].floatValue, 1.5), "JKL.COG.ResourceLocator: bar.floatValue");

                check(cog.values[2].name == "baz", "JKL.COG.ResourceLocator: baz.name");
                check(cog.values[2].type == sed::ValueType::vect, "JKL.COG.ResourceLocator: baz.type");
                check(cog.values[2].stringValue == "(1/2/3)", "JKL.COG.ResourceLocator: baz.stringValue");

                check(cog.values[3].name == "sec", "JKL.COG.ResourceLocator: sec.name");
                check(cog.values[3].type == sed::ValueType::ptr, "JKL.COG.ResourceLocator: sec.type");
                check(cog.values[3].intValue == 2, "JKL.COG.ResourceLocator: sec.intValue");

                check(cog.values[4].name == "surf", "JKL.COG.ResourceLocator: surf.name");
                check(cog.values[4].type == sed::ValueType::ptr, "JKL.COG.ResourceLocator: surf.type");
                check(cog.values[4].intValue == 3, "JKL.COG.ResourceLocator: surf.intValue");

                check(cog.values[5].name == "thg", "JKL.COG.ResourceLocator: thg.name");
                check(cog.values[5].type == sed::ValueType::ptr, "JKL.COG.ResourceLocator: thg.type");
                check(cog.values[5].intValue == 4, "JKL.COG.ResourceLocator: thg.intValue");

                check(cog.values[6].name == "other", "JKL.COG.ResourceLocator: other.name");
                check(cog.values[6].type == sed::ValueType::integer, "JKL.COG.ResourceLocator: other.type");
                check(cog.values[6].intValue == 7, "JKL.COG.ResourceLocator: other.intValue");
            }
        }

        std::filesystem::remove_all(root, ec);
    }

    {
        // JKL VERSION 1 cannot distinguish JKDF2 from MotS until the
        // resource-independent pass has decoded the complete level.  The
        // deferred resource pass must still restore COG schemas from the
        // authoritative game's resources, including pointer types used by
        // geometry-reference remapping.
        const auto unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path jkdf2Root =
            "tmp_jkl_deferred_jkdf2_" + unique;
        const std::filesystem::path motsRoot =
            "tmp_jkl_deferred_mots_" + unique;
        std::error_code ec;
        std::filesystem::create_directories(jkdf2Root / "cog", ec);
        check(!ec, "JKL.DeferredCog: JKDF2 resource directory creates");
        ec.clear();
        std::filesystem::create_directories(motsRoot / "cog", ec);
        check(!ec, "JKL.DeferredCog: MotS resource directory creates");
        {
            std::ofstream out(jkdf2Root / "cog" / "target.cog");
            out << "symbols\n"
                << "    sector target\n"
                << "end\n"
                << "code\n"
                << "end\n";
        }
        {
            // Deliberately use the same script name with a different schema.
            // Resolving against an extension-based provisional locator would
            // therefore be observably wrong.
            std::ofstream out(motsRoot / "cog" / "target.cog");
            out << "symbols\n"
                << "    int target\n"
                << "end\n"
                << "code\n"
                << "end\n";
        }

        const char* jkl = R"JKL(
        SECTION: HEADER
        VERSION 1
        END
        SECTION: GEORESOURCE
        WORLD COLORMAPS 0
        WORLD VERTICES 0
        WORLD TEXTURE VERTICES 0
        WORLD ADJOINS 0
        WORLD SURFACES 0
        END
        SECTION: SECTORS
        WORLD SECTORS 2
        SECTOR 0
        FLAGS 0
        VERTICES 0
        SURFACES 0 0
        SECTOR 1
        FLAGS 0
        VERTICES 0
        SURFACES 0 0
        END
        SECTION: COGS
        WORLD COGS 1
        0: target.cog 1
        END
        )JKL";

        sed::assets::ResourceLocator jkdf2Resources{};
        jkdf2Resources.addRoot(jkdf2Root);
        sed::assets::JklLoadOptions jkdf2Options{};
        jkdf2Options.projectType = sed::ProjectType::JKDF2;
        std::istringstream jkdf2Input(jkl);
        sed::Level jkdf2Level = sed::assets::loadFromJKL(jkdf2Input, jkdf2Options);
        check(jkdf2Level.cogs.size() == 1 && !jkdf2Level.cogs[0].values.empty(),
            "JKL.DeferredCog: JKDF2 resource-independent pass keeps raw argument");
        if (jkdf2Level.cogs.size() == 1 && !jkdf2Level.cogs[0].values.empty())
        {
            check(jkdf2Level.cogs[0].values[0].name == "param0"
                    && jkdf2Level.cogs[0].values[0].type == sed::ValueType::str
                    && jkdf2Level.cogs[0].values[0].stringValue == "1",
                "JKL.DeferredCog: JKDF2 first pass preserves raw wire token");
        }
        sed::assets::finalizeJklImportResources(jkdf2Level, jkdf2Resources);
        check(jkdf2Level.cogs.size() == 1 && jkdf2Level.cogs[0].values.size() == 1,
            "JKL.DeferredCog: JKDF2 finalizer keeps one argument");
        if (jkdf2Level.cogs.size() == 1 && jkdf2Level.cogs[0].values.size() == 1)
        {
            const sed::CogValue& target = jkdf2Level.cogs[0].values[0];
            check(target.name == "target" && target.cogType == sed::CogType::sec
                    && target.type == sed::ValueType::ptr && target.intValue == 1,
                "JKL.DeferredCog: JKDF2 finalizer restores typed sector reference");
            check(sed::level::levelDeleteSector(jkdf2Level, 0),
                "JKL.DeferredCog: deleting an earlier sector succeeds");
            check(jkdf2Level.cogs[0].values[0].intValue == 0,
                "JKL.DeferredCog: typed sector reference remaps after delete");
        }

        sed::assets::ResourceLocator motsResources{};
        motsResources.addRoot(motsRoot);
        sed::assets::JklLoadOptions motsOptions{};
        motsOptions.projectType = sed::ProjectType::MOTS;
        std::istringstream motsInput(jkl);
        sed::Level motsLevel = sed::assets::loadFromJKL(motsInput, motsOptions);
        sed::assets::finalizeJklImportResources(motsLevel, motsResources);
        check(motsLevel.cogs.size() == 1 && motsLevel.cogs[0].values.size() == 1,
            "JKL.DeferredCog: MotS finalizer keeps one argument");
        if (motsLevel.cogs.size() == 1 && motsLevel.cogs[0].values.size() == 1)
        {
            const sed::CogValue& target = motsLevel.cogs[0].values[0];
            check(target.name == "target" && target.cogType == sed::CogType::int_
                    && target.type == sed::ValueType::integer && target.intValue == 1,
                "JKL.DeferredCog: MotS finalizer uses the authoritative same-name schema");
        }

        std::string missingScriptJkl(jkl);
        const std::size_t scriptName = missingScriptJkl.find("target.cog");
        if (scriptName != std::string::npos)
        {
            missingScriptJkl.replace(scriptName, std::string_view("target.cog").size(),
                "missing.cog");
        }
        std::istringstream missingScriptInput(missingScriptJkl);
        sed::Level missingScriptLevel = sed::assets::loadFromJKL(
            missingScriptInput);
        sed::assets::finalizeJklImportResources(missingScriptLevel, jkdf2Resources);
        check(missingScriptLevel.cogs.size() == 1
                && missingScriptLevel.cogs[0].values.size() == 1
                && missingScriptLevel.cogs[0].values[0].name == "param0"
                && missingScriptLevel.cogs[0].values[0].type == sed::ValueType::str
                && missingScriptLevel.cogs[0].values[0].stringValue == "1",
            "JKL.DeferredCog: missing script preserves the raw argument and generic schema");
        std::ostringstream missingScriptExport;
        sed::assets::saveToJKL(missingScriptLevel, missingScriptExport);
        bool missingScriptRowExact = false;
        std::istringstream missingScriptRows(missingScriptExport.str());
        std::string missingScriptRow;
        while (std::getline(missingScriptRows, missingScriptRow))
        {
            missingScriptRowExact = missingScriptRowExact
                || missingScriptRow == "0:\tmissing.cog\t1";
        }
        check(missingScriptRowExact,
            "JKL.DeferredCog: missing script exports the original wire argument");

        std::filesystem::remove_all(jkdf2Root, ec);
        std::filesystem::remove_all(motsRoot, ec);
    }

    {
        // Surface records have version-dependent fixed fields.  Validate the
        // boundary before indexing the extra-light and vertex-count tokens,
        // then reject invalid counts and incomplete index pairs explicitly.
        const auto makeJkl = [](const int version, const std::string_view record)
        {
            std::ostringstream out;
            out << "SECTION: HEADER\n"
                << "VERSION " << version << "\n"
                << "END\n"
                << "SECTION: GEORESOURCE\n"
                << (version == 1 ? "WORLD COLORMAPS 0\n" : "")
                << "WORLD VERTICES 3\n"
                << "0: 0 0 0\n"
                << "1: 1 0 0\n"
                << "2: 0 1 0\n"
                << "WORLD TEXTURE VERTICES 3\n"
                << "0: 0 0\n"
                << "1: 1 0\n"
                << "2: 0 1\n"
                << "WORLD ADJOINS 0\n"
                << "WORLD SURFACES 1\n"
                << (record.empty() ? std::string{} : std::string(record) + "\n")
                << "END\n"
                << "SECTION: SECTORS\n"
                << "WORLD SECTORS 1\n"
                << "SECTOR 0\n"
                << "FLAGS 0\n"
                << "VERTICES 3\n"
                << "0: 0\n"
                << "1: 1\n"
                << "2: 2\n"
                << "SURFACES 0 1\n"
                << "END\n";
            return out.str();
        };
        constexpr std::string_view fixedError =
            "Cannot load JKL/NDY surface: fixed fields are truncated for the declared version.";
        constexpr std::string_view countError =
            "Cannot load JKL/NDY surface: vertex count is invalid.";
        constexpr std::string_view pairError =
            "Cannot load JKL/NDY surface: vertex index pairs are truncated.";
        constexpr std::string_view geoCountError =
            "Cannot load JKL/NDY GEORESOURCE count: count is invalid.";
        constexpr std::string_view geoRecordsError =
            "Cannot load JKL/NDY GEORESOURCE count: records are truncated.";

        const auto makeGeoCountJkl = [](
            const int version,
            const std::size_t countIndex,
            const std::string_view countToken)
        {
            const std::array<std::string_view, 5> v1Labels{
                "WORLD COLORMAPS", "WORLD VERTICES",
                "WORLD TEXTURE VERTICES", "WORLD ADJOINS", "WORLD SURFACES"};
            const std::array<std::string_view, 4> ijimLabels{
                "WORLD VERTICES", "WORLD TEXTURE VERTICES",
                "WORLD ADJOINS", "WORLD SURFACES"};
            std::ostringstream out;
            out << "SECTION: HEADER\n"
                << "VERSION " << version << "\n"
                << "END\n"
                << "SECTION: GEORESOURCE\n";
            if (version == 1)
            {
                for (std::size_t index = 0; index < v1Labels.size(); ++index)
                {
                    const std::string_view value = index == countIndex
                        ? countToken : std::string_view{"0"};
                    out << v1Labels[index] << ' '
                        << value << "\n";
                }
            }
            else
            {
                for (std::size_t index = 0; index < ijimLabels.size(); ++index)
                {
                    const std::string_view value = index == countIndex
                        ? countToken : std::string_view{"0"};
                    out << ijimLabels[index] << ' '
                        << value << "\n";
                }
            }
            out << "END\n";
            return out.str();
        };

        const auto makeTriangleRecord = [](
            const int version,
            const std::string_view pairEncoding)
        {
            const std::string fixed = version == 1
                ? "0: -1 0x0 0x0 0 0 0 -1 0 3 "
                : "0: -1 0x0 0x0 0 0 0 -1 1 2 3 0.5 3 ";
            if (pairEncoding == "comma")
            {
                return fixed + "0,0 1,1 2,2";
            }
            if (pairEncoding == "comma-space")
            {
                return fixed + "0, 0 1, 1 2, 2";
            }
            return fixed + "0 0 1 1 2 2";
        };

        for (const int version : {1, 2, 3})
        {
            for (const std::string_view pairEncoding : {
                     "comma", "comma-space", "space"})
            {
                std::istringstream input(makeJkl(
                    version, makeTriangleRecord(version, pairEncoding)));
                const sed::Level level = sed::assets::loadFromJKL(input);
                const bool loaded = level.sectors.size() == 1
                    && level.sectors[0].surfaces.size() == 1
                    && level.sectors[0].surfaces[0].vertices
                        == std::vector<std::int32_t>{0, 1, 2}
                    && level.sectors[0].surfaces[0].texVertices.size() == 3
                    && close(level.sectors[0].vertices[1].x, 1.0)
                    && close(level.sectors[0].vertices[2].y, 1.0);
                check(loaded,
                    "JKL.SurfaceBounds: valid minimum triangle imports positions and pairs");
                if (loaded)
                {
                    const ColorF& extra = level.sectors[0].surfaces[0].extraLight;
                    if (version == 1)
                    {
                        check(close(extra.r, 0.0) && close(extra.g, 0.0)
                                && close(extra.b, 0.0) && close(extra.a, 1.0),
                            "JKL.SurfaceBounds: VERSION 1 scalar extra light imports");
                    }
                    else
                    {
                        check(close(extra.r, 1.0) && close(extra.g, 2.0)
                                && close(extra.b, 3.0) && close(extra.a, 0.5),
                            "JKL.SurfaceBounds: VERSION 2/3 RGBA extra light imports");
                    }
                }
            }
        }
        for (const int version : {1, 2, 3})
        {
            const std::size_t fixedFieldCount = version == 1 ? 10u : 13u;
            for (std::size_t tokenCount = 1;
                tokenCount < fixedFieldCount;
                ++tokenCount)
            {
                std::string record = "0:";
                for (std::size_t token = 1; token < tokenCount; ++token)
                {
                    record += " 0";
                }
                checkThrowsWithMessage(
                    [&]()
                    {
                        std::istringstream input(makeJkl(version, record));
                        (void)sed::assets::loadFromJKL(input);
                    },
                    fixedError,
                    "JKL.SurfaceBounds: fixed fields reject every truncated prefix");
            }
        }

        for (const int version : {1, 2, 3})
        {
            const std::size_t countLines = version == 1 ? 5u : 4u;
            for (std::size_t countIndex = 0;
                countIndex < countLines;
                ++countIndex)
            {
                for (const std::string_view countToken : {"-1", "invalid"})
                {
                    checkThrowsWithMessage(
                        [&]()
                        {
                            std::istringstream input(makeGeoCountJkl(
                                version, countIndex, countToken));
                            (void)sed::assets::loadFromJKL(input);
                        },
                        geoCountError,
                        "JKL.GeoResourceBounds: invalid count has the fixed diagnostic for every version and record class");
                }
                checkThrowsWithMessage(
                    [&]()
                    {
                        std::istringstream input(makeGeoCountJkl(
                            version, countIndex, "2147483647"));
                        (void)sed::assets::loadFromJKL(input);
                    },
                    geoRecordsError,
                    "JKL.GeoResourceBounds: overlarge count is bounded before reserve for every version and record class");
            }
        }

        for (const int version : {1, 2, 3})
        {
            for (const std::string_view countToken : {"-1", "invalid"})
            {
                const std::string fixed = version == 1
                    ? "0: -1 0x0 0x0 0 0 0 -1 0 "
                    : "0: -1 0x0 0x0 0 0 0 -1 0 0 0 0 ";
                checkThrowsWithMessage(
                    [&]()
                    {
                        std::istringstream input(makeJkl(
                            version, fixed + std::string(countToken)));
                        (void)sed::assets::loadFromJKL(input);
                    },
                    countError,
                    "JKL.SurfaceBounds: invalid vertex count has the fixed diagnostic for every version");
            }
        }

        for (const int version : {1, 2, 3})
        {
            for (const std::string_view pairs : {"0,", "0", "0 0", "0,0 1"})
            {
                const std::string fixed = version == 1
                    ? "0: -1 0x0 0x0 0 0 0 -1 0 2 "
                    : "0: -1 0x0 0x0 0 0 0 -1 0 0 0 0 2 ";
                const std::string record = fixed + std::string(pairs);
                checkThrowsWithMessage(
                    [&]()
                    {
                        std::istringstream input(makeJkl(version, record));
                        (void)sed::assets::loadFromJKL(input);
                    },
                    pairError,
                    "JKL.SurfaceBounds: truncated vertex pair has the fixed diagnostic for every version");
            }
        }

        for (const int version : {1, 2, 3})
        {
            checkThrowsWithMessage(
                [&]()
                {
                    std::istringstream input(makeJkl(version, {}));
                    (void)sed::assets::loadFromJKL(input);
                },
                geoRecordsError,
                    "JKL.GeoResourceBounds: missing surface records have the fixed diagnostic");
        }
    }
}

} // namespace sed::tests
