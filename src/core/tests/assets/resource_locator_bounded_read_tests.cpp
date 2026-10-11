#include "test_common.hpp"

#include "sed/assets/bounded_stream_reader.hpp"
#include "sed/assets/resource_locator.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

struct TemporaryResources
{
    std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("sed-resource-locator-bounded-"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    TemporaryResources()
    {
        std::filesystem::create_directories(root);
    }

    ~TemporaryResources()
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};

bool writeFile(const std::filesystem::path& path, const std::string_view bytes)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if ( ec )
    {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if ( !out )
    {
        return false;
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

void writeU32LE(std::ostream& out, const std::uint32_t value)
{
    for ( unsigned int shift = 0; shift < 32; shift += 8 )
    {
        out.put(static_cast<char>((value >> shift) & 0xffu));
    }
}

bool writeGobV2(
    const std::filesystem::path& path,
    const std::vector<std::pair<std::string, std::string>>& entries)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if ( !out )
    {
        return false;
    }

    out.write("GOB ", 4);
    writeU32LE(out, 0x14u);
    writeU32LE(out, 0x0cu);
    writeU32LE(out, static_cast<std::uint32_t>(entries.size()));

    std::uint32_t dataOffset = 16u + static_cast<std::uint32_t>(entries.size()) * 136u;
    for ( const auto& [name, data] : entries )
    {
        if ( name.size() > 128u )
        {
            return false;
        }

        writeU32LE(out, dataOffset);
        writeU32LE(out, static_cast<std::uint32_t>(data.size()));
        std::array<char, 128> nameBytes{};
        std::copy(name.begin(), name.end(), nameBytes.begin());
        out.write(nameBytes.data(), static_cast<std::streamsize>(nameBytes.size()));
        dataOffset += static_cast<std::uint32_t>(data.size());
    }

    for ( const auto& [name, data] : entries )
    {
        (void)name;
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    return static_cast<bool>(out);
}

bool bytesEqual(const std::vector<std::byte>& bytes, const std::string_view expected)
{
    if ( bytes.size() != expected.size() )
    {
        return false;
    }
    for ( std::size_t i = 0; i < bytes.size(); ++i )
    {
        if ( std::to_integer<unsigned char>(bytes[i])
            != static_cast<unsigned char>(expected[i]) )
        {
            return false;
        }
    }
    return true;
}

class FailingAfterBytesStreamBuffer final : public std::streambuf
{
public:
    explicit FailingAfterBytesStreamBuffer(const std::string_view prefix)
        : prefix_(prefix)
    {
        char* const begin = prefix_.data();
        setg(begin, begin, begin + prefix_.size());
    }

protected:
    int_type underflow() override
    {
        throw std::runtime_error("deterministic stream failure");
    }

private:
    std::string prefix_{};
};

void runBoundedStreamStaleHintRegressions()
{
    bool exceededBudget = false;
    std::istringstream zeroHintInput("grew");
    const auto zeroHint = sed::assets::detail::readBinaryStreamWithinBudget<std::string>(
        zeroHintInput,
        16u,
        0u,
        exceededBudget,
        "stream read failed");
    check(zeroHint && *zeroHint == "grew" && !exceededBudget,
        "bounded stream read includes bytes when the size hint was zero");

    std::istringstream grewWithinBudgetInput("complete source");
    const auto grewWithinBudget = sed::assets::detail::readBinaryStreamWithinBudget<std::string>(
        grewWithinBudgetInput,
        32u,
        3u,
        exceededBudget,
        "stream read failed");
    check(grewWithinBudget && *grewWithinBudget == "complete source" && !exceededBudget,
        "bounded stream read includes all bytes when the opened stream grew within budget");

    std::istringstream grewOverBudgetInput("123456");
    const auto grewOverBudget = sed::assets::detail::readBinaryStreamWithinBudget<std::string>(
        grewOverBudgetInput,
        5u,
        2u,
        exceededBudget,
        "stream read failed");
    check(!grewOverBudget && exceededBudget,
        "bounded stream read rejects growth beyond its byte budget");
    check(grewOverBudgetInput.tellg() == std::streampos(6),
        "bounded stream read consumes no more than one byte beyond its budget");

    std::istringstream exactLimitInput("data");
    const auto exactLimit = sed::assets::detail::readBinaryStreamWithinBudget<std::string>(
        exactLimitInput,
        4u,
        4u,
        exceededBudget,
        "stream read failed");
    check(exactLimit && *exactLimit == "data" && !exceededBudget,
        "bounded stream read accepts an exact-budget payload");

    std::istringstream shrunkHintInput("shrunk");
    const auto shrunkHint = sed::assets::detail::readBinaryStreamWithinBudget<std::string>(
        shrunkHintInput,
        8u,
        64u,
        exceededBudget,
        "stream read failed");
    check(shrunkHint && *shrunkHint == "shrunk" && !exceededBudget,
        "bounded stream read accepts a complete stream shorter than its stale size hint");

    std::istringstream emptyAtZeroBudgetInput("");
    const auto emptyAtZeroBudget = sed::assets::detail::readBinaryStreamWithinBudget<std::string>(
        emptyAtZeroBudgetInput,
        0u,
        0u,
        exceededBudget,
        "stream read failed");
    check(emptyAtZeroBudget && emptyAtZeroBudget->empty() && !exceededBudget,
        "bounded stream read accepts an empty payload at a zero-byte limit");

    FailingAfterBytesStreamBuffer failingBuffer("partial");
    std::istream failingInput(&failingBuffer);
    checkThrowsWithMessage(
        [&]()
        {
            (void)sed::assets::detail::readBinaryStreamWithinBudget<std::string>(
                failingInput,
                32u,
                7u,
                exceededBudget,
                "stream read failed");
        },
        "stream read failed",
        "bounded stream read propagates a deterministic non-EOF stream failure");
}

template <typename Locator>
concept SupportsTwoArgumentBoundedBinaryReads = requires(
    const Locator& locator,
    const std::string_view name,
    const std::size_t maxBytes)
{
    { locator.tryReadBinary(name, maxBytes) }
        -> std::same_as<std::optional<std::vector<std::byte>>>;
    { locator.readBinary(name, maxBytes) }
        -> std::same_as<std::vector<std::byte>>;
};

template <typename Locator>
concept ReportsWinningLooseSource = requires(
    const Locator& locator,
    const std::string_view name,
    const std::size_t maxBytes,
    std::filesystem::path* diskSource)
{
    { locator.tryReadBinary(name, maxBytes, diskSource) }
        -> std::same_as<std::optional<std::vector<std::byte>>>;
};

template <typename Locator>
concept ReportsSelectedLooseWinner = requires(
    const Locator& locator,
    const std::string_view name)
{
    { locator.findSelectedOnDisk(name) }
        -> std::same_as<std::optional<std::filesystem::path>>;
};

template <typename Locator>
void runBoundedBinaryReadContract()
{
    if constexpr ( !SupportsTwoArgumentBoundedBinaryReads<Locator> )
    {
        check(false, "ResourceLocator exposes explicit byte-budget binary reads");
        return;
    }
    else if constexpr ( !ReportsWinningLooseSource<Locator> )
    {
        check(false, "ResourceLocator bounded reads report the winning loose-file source path");
        return;
    }
    else if constexpr ( !ReportsSelectedLooseWinner<Locator> )
    {
        check(false, "ResourceLocator reports the loose source only when it wins candidate precedence");
        return;
    }
    else
    {
        TemporaryResources fixture{};
        const auto looseRoot = fixture.root / "loose";
        check(writeFile(looseRoot / "under.bin", "12345"),
            "ResourceLocator.bounded.loose writes a small resource");
        check(writeFile(looseRoot / "empty.bin", ""),
            "ResourceLocator.bounded.loose writes an empty resource");
        check(writeFile(looseRoot / "same.bin", "LOOSE"),
            "ResourceLocator.bounded.loose writes a precedence resource");
        check(writeFile(looseRoot / "mat" / "material.mat", "small"),
            "ResourceLocator.bounded.loose writes a later candidate fallback");
        check(writeFile(looseRoot / "primary.txt", "DISK"),
            "ResourceLocator.bounded.loose writes an override fallback");

        const auto sparsePath = looseRoot / "sparse.bin";
        constexpr std::uint64_t kSparseSize = 8u * 1024u * 1024u;
        {
            std::ofstream sparse(sparsePath, std::ios::binary | std::ios::trunc);
            sparse.seekp(static_cast<std::streamoff>(kSparseSize - 1u));
            sparse.put('\0');
            check(static_cast<bool>(sparse),
                "ResourceLocator.bounded.loose creates a sparse large resource");
        }

        const auto firstGobPath = fixture.root / "first.gob";
        const auto secondGobPath = fixture.root / "second.gob";
        check(writeGobV2(firstGobPath,
                {
                    { "gob.bin", "OLD" },
                    { "same.bin", "ARCHIVE" },
                    { "material.mat", "ARCHIVE-WINNER" },
                    { "mount.bin", "FIRST" },
                }),
            "ResourceLocator.bounded.gob writes a synthetic first archive");
        check(writeGobV2(secondGobPath,
                {
                    { "gob.bin", "LATEST" },
                    { "empty-gob.bin", "" },
                    { "mount.bin", "LATEST-GOB" },
                }),
            "ResourceLocator.bounded.gob writes a synthetic second archive");

        Locator locator{};
        locator.addRoot(looseRoot);

        const auto under = locator.tryReadBinary("under.bin", 6u);
        const auto exact = locator.tryReadBinary("under.bin", 5u);
        const auto over = locator.tryReadBinary("under.bin", 4u);
        bool exceededBudget = false;
        const auto overWithStatus = locator.tryReadBinary("under.bin", 4u, exceededBudget);
        check(under && bytesEqual(*under, "12345"),
            "ResourceLocator.bounded.loose accepts a budget above the payload size");
        check(exact && bytesEqual(*exact, "12345"),
            "ResourceLocator.bounded.loose accepts a budget equal to the payload size");
        check(!over,
            "ResourceLocator.bounded.loose refuses a payload above its byte budget");
        check(!overWithStatus && exceededBudget,
            "ResourceLocator.bounded reports an oversized selected winner separately from absence");

        std::filesystem::path diskSource = fixture.root / "stale-source.bin";
        const auto selectedLoose = locator.tryReadBinary("under.bin", 5u, &diskSource);
        check(selectedLoose && bytesEqual(*selectedLoose, "12345")
                && diskSource == looseRoot / "under.bin",
            "ResourceLocator.bounded reports the exact selected loose-file path");

        const auto absoluteLoosePath = looseRoot / "under.bin";
        diskSource = fixture.root / "stale-source.bin";
        const auto selectedAbsoluteLoose = locator.tryReadBinary(
            absoluteLoosePath.string(), 5u, &diskSource);
        check(selectedAbsoluteLoose && bytesEqual(*selectedAbsoluteLoose, "12345")
                && diskSource == absoluteLoosePath,
            "ResourceLocator.bounded reports an absolute loose-file source path");
        const auto withoutDiskSource = locator.tryReadBinary("under.bin", 5u, nullptr);
        check(withoutDiskSource && bytesEqual(*withoutDiskSource, "12345"),
            "ResourceLocator.bounded accepts an explicit null source-path output");

        Locator aliasLocator{};
        aliasLocator.addRoot(looseRoot);
        diskSource = fixture.root / "stale-source.bin";
        const auto selectedAlias = aliasLocator.tryReadBinary("mat/material.mat", 5u, &diskSource);
        check(selectedAlias && bytesEqual(*selectedAlias, "small")
                && diskSource == looseRoot / "mat" / "material.mat",
            "ResourceLocator.bounded reports the selected loose path for an authored candidate alias");
        check(aliasLocator.findSelectedOnDisk("mat/material.mat")
                == std::optional<std::filesystem::path>(looseRoot / "mat" / "material.mat"),
            "ResourceLocator metadata reports a loose candidate alias when it wins");

        const auto empty = locator.tryReadBinary("empty.bin", 0u);
        check(empty && empty->empty(),
            "ResourceLocator.bounded.loose accepts an empty resource with a zero-byte budget");
        diskSource = fixture.root / "stale-source.bin";
        check(!locator.tryReadBinary("absent.bin", 12u, &diskSource) && diskSource.empty(),
            "ResourceLocator.bounded.loose preserves missing-resource behavior");
        exceededBudget = true;
        check(!locator.tryReadBinary("absent.bin", 12u, exceededBudget) && !exceededBudget,
            "ResourceLocator.bounded reports a true miss without marking a budget refusal");
        checkThrowsWithMessage(
            [&]() { (void)locator.readBinary("absent.bin", 12u); },
            "Resource not found: absent.bin",
            "ResourceLocator.bounded.readBinary throws when the resource is absent");

        std::error_code sizeError;
        const auto sparseSize = std::filesystem::file_size(sparsePath, sizeError);
        const auto sparse = locator.tryReadBinary("sparse.bin", 8u);
        check(!sizeError && sparseSize == kSparseSize && !sparse,
            "ResourceLocator.bounded.loose refuses a sparse file before accepting its payload");

        diskSource = fixture.root / "stale-source.bin";
        check(!locator.tryReadBinary("under.bin", 4u, &diskSource) && diskSource.empty(),
            "ResourceLocator.bounded clears the source path when a loose winner exceeds the budget");

        checkThrowsWithMessage(
            [&]() { (void)locator.readBinary("under.bin", 4u); },
            "Resource exceeds byte budget: under.bin",
            "ResourceLocator.bounded.readBinary throws when the selected resource exceeds its budget");

        bool firstMounted = false;
        bool secondMounted = false;
        try
        {
            locator.mountGob(firstGobPath);
            firstMounted = true;
            locator.mountGob(secondGobPath);
            secondMounted = true;
        }
        catch ( const std::exception& )
        {
        }
        check(firstMounted,
            "ResourceLocator.bounded mounts the first synthetic GOB");
        check(secondMounted,
            "ResourceLocator.bounded mounts the second synthetic GOB");

        diskSource = fixture.root / "stale-source.bin";
        const auto gobUnder = locator.tryReadBinary("gob.bin", 7u, &diskSource);
        const auto gobExact = locator.tryReadBinary("gob.bin", 6u);
        check(gobUnder && bytesEqual(*gobUnder, "LATEST"),
            "ResourceLocator.bounded.gob accepts an under-budget selected archive entry");
        check(diskSource.empty(),
            "ResourceLocator.bounded clears the source path when a GOB entry wins");
        check(gobExact && bytesEqual(*gobExact, "LATEST"),
            "ResourceLocator.bounded.gob accepts an exact-budget selected archive entry");
        diskSource = fixture.root / "stale-source.bin";
        check(!locator.tryReadBinary("gob.bin", 5u, &diskSource) && diskSource.empty(),
            "ResourceLocator.bounded.gob refuses an over-budget newest archive entry");

        const auto emptyGob = locator.tryReadBinary("empty-gob.bin", 0u);
        check(emptyGob && emptyGob->empty(),
            "ResourceLocator.bounded.gob accepts an empty entry with a zero-byte budget");

        diskSource = fixture.root / "stale-source.bin";
        const auto sameCandidate = locator.tryReadBinary("same.bin", 5u, &diskSource);
        check(sameCandidate && bytesEqual(*sameCandidate, "LOOSE"),
            "ResourceLocator.bounded keeps loose-file precedence over a GOB entry for one candidate");
        check(diskSource == looseRoot / "same.bin",
            "ResourceLocator.bounded reports the loose source when a same-candidate loose file wins");
        check(locator.findSelectedOnDisk("same.bin")
                == std::optional<std::filesystem::path>(looseRoot / "same.bin"),
            "ResourceLocator selected-source metadata preserves loose precedence within one candidate");
        diskSource = fixture.root / "stale-source.bin";
        check(!locator.tryReadBinary("same.bin", 4u, &diskSource) && diskSource.empty(),
            "ResourceLocator.bounded does not fall back from an oversized loose winner to its GOB shadow");

        diskSource = looseRoot / "mat" / "material.mat";
        const auto earlierCandidate = locator.tryReadBinary("material.mat", 14u, &diskSource);
        check(earlierCandidate && bytesEqual(*earlierCandidate, "ARCHIVE-WINNER"),
            "ResourceLocator.bounded keeps an earlier exact GOB candidate ahead of a later loose alias");
        check(diskSource.empty(),
            "ResourceLocator.bounded clears a stale later-alias path when an earlier GOB candidate wins");
        check(locator.findOnDisk("material.mat")
                == std::optional<std::filesystem::path>(looseRoot / "mat" / "material.mat")
                && !locator.findSelectedOnDisk("material.mat"),
            "ResourceLocator selected-source metadata stops at an earlier exact GOB candidate before a loose alias");
        diskSource = looseRoot / "mat" / "material.mat";
        check(!locator.tryReadBinary("material.mat", 5u, &diskSource) && diskSource.empty(),
            "ResourceLocator.bounded does not fall back from an oversized earlier candidate");

        diskSource = fixture.root / "stale-source.bin";
        const auto mountExact = locator.tryReadBinary("mount.bin", 10u, &diskSource);
        check(mountExact && bytesEqual(*mountExact, "LATEST-GOB"),
            "ResourceLocator.bounded keeps reverse mount precedence among matching GOB entries");
        check(diskSource.empty(),
            "ResourceLocator.bounded leaves the source path empty for the selected GOB mount");
        diskSource = fixture.root / "stale-source.bin";
        check(!locator.tryReadBinary("mount.bin", 5u, &diskSource) && diskSource.empty(),
            "ResourceLocator.bounded does not fall back to an older smaller GOB entry after budget refusal");

        int overrideReads = 0;
        locator.setPrimaryResourceOverride("primary.txt", [&]()
            -> std::optional<std::vector<std::byte>>
        {
            ++overrideReads;
            const std::string bytes = "PRIMARY-OVERRIDE";
            std::vector<std::byte> result(bytes.size());
            std::transform(bytes.begin(), bytes.end(), result.begin(), [](const char ch)
            {
                return static_cast<std::byte>(static_cast<unsigned char>(ch));
            });
            return result;
        });

        check(!locator.findSelectedOnDisk("primary.txt") && overrideReads == 0,
            "ResourceLocator selected-source metadata treats a primary override as non-loose without materializing it");

        diskSource = fixture.root / "stale-source.bin";
        const auto tooLargeOverride = locator.tryReadBinary("primary.txt", 4u, &diskSource);
        check(!tooLargeOverride && overrideReads == 1 && diskSource.empty(),
            "ResourceLocator.bounded checks a trusted primary callback result after materialization");
        diskSource = fixture.root / "stale-source.bin";
        check(!locator.tryReadBinary("primary.txt", 4u, &diskSource)
                && overrideReads == 1 && diskSource.empty(),
            "ResourceLocator.bounded does not fall back or rematerialize an oversized primary result");
        diskSource = fixture.root / "stale-source.bin";
        const auto acceptedOverride = locator.tryReadBinary("primary.txt", 16u, &diskSource);
        check(acceptedOverride && bytesEqual(*acceptedOverride, "PRIMARY-OVERRIDE")
                && overrideReads == 1 && diskSource.empty(),
            "ResourceLocator.bounded accepts a cached primary callback result within the budget");

        Locator throwingOverrideLocator{};
        throwingOverrideLocator.setPrimaryResourceOverride("broken.txt", []()
            -> std::optional<std::vector<std::byte>>
        {
            throw std::runtime_error("deterministic selected override failure");
        });
        diskSource = fixture.root / "stale-source.bin";
        checkThrowsWithMessage(
            [&]()
            {
                (void)throwingOverrideLocator.tryReadBinary("broken.txt", 32u, &diskSource);
            },
            "deterministic selected override failure",
            "ResourceLocator.bounded propagates primary callback failures");
        check(diskSource.empty(),
            "ResourceLocator.bounded clears the source path when a primary callback throws");

        const auto failedFirstGobPath = fixture.root / "failed-first.gob";
        const auto failedLastGobPath = fixture.root / "failed-last.gob";
        check(writeGobV2(failedFirstGobPath, { { "failure.bin", "FALLBACK" } }),
            "ResourceLocator.bounded.gob writes an older read-failure fallback");
        check(writeGobV2(failedLastGobPath, { { "failure.bin", "BROKEN" } }),
            "ResourceLocator.bounded.gob writes the selected read-failure source");
        Locator failedReadLocator{};
        bool failedReadMountsSucceeded = false;
        try
        {
            failedReadLocator.mountGob(failedFirstGobPath);
            failedReadLocator.mountGob(failedLastGobPath);
            failedReadMountsSucceeded = true;
        }
        catch ( const std::exception& )
        {
        }
        check(failedReadMountsSucceeded,
            "ResourceLocator.bounded mounts synthetic archives for the failed-read case");
        check(writeFile(failedLastGobPath, ""),
            "ResourceLocator.bounded truncates the selected archive after indexing");
        check(!failedReadLocator.findSelectedOnDisk("failure.bin"),
            "ResourceLocator selected-source metadata recognizes an indexed GOB winner without reading its payload");
        checkThrowsWithMessage(
            [&]()
            {
                diskSource = fixture.root / "stale-source.bin";
                (void)failedReadLocator.tryReadBinary("failure.bin", 6u, &diskSource);
            },
            "Unexpected EOF while reading GOB entry: failure.bin",
            "ResourceLocator.bounded propagates selected GOB read failures without trying a shadowed entry");
        check(diskSource.empty(),
            "ResourceLocator.bounded clears the source path before a selected read throws");
    }
}

} // namespace

void runResourceLocatorBoundedReadTests()
{
    runBoundedStreamStaleHintRegressions();
    runBoundedBinaryReadContract<sed::assets::ResourceLocator>();
}

} // namespace sed::tests
