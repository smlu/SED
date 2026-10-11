#include "test_common.hpp"

#include "sed/binary_reader.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sed::tests
{

void runBinaryReaderTests()
{
    const std::array bytes{
        std::byte{ 0x7f },
        std::byte{ 0x34 },
        std::byte{ 0x12 },
        std::byte{ 0x78 },
        std::byte{ 0x56 },
        std::byte{ 0x34 },
        std::byte{ 0x12 },
        std::byte{ 'A' },
        std::byte{ 0x00 },
        std::byte{ 'B' },
    };
    ByteReader reader{ std::span<const std::byte>(bytes) };

    check(reader.position() == 0 && reader.remaining() == bytes.size(),
        "ByteReader: initial position and remaining count");
    check(reader.readLE<std::uint8_t>() == 0x7fu,
        "ByteReader: reads one-byte unsigned value");
    check(reader.readLE<std::uint16_t>() == 0x1234u,
        "ByteReader: reads two-byte little-endian value");
    check(reader.readLE<std::uint32_t>() == 0x12345678u,
        "ByteReader: reads four-byte little-endian value");

    const std::string_view embeddedNull = reader.readString(3);
    check(embeddedNull.size() == 3
            && embeddedNull[0] == 'A'
            && embeddedNull[1] == '\0'
            && embeddedNull[2] == 'B',
        "ByteReader: string views preserve embedded null bytes");
    check(reader.position() == bytes.size() && reader.remaining() == 0,
        "ByteReader: exact-end reads consume the input");
    check(reader.readBytes(0).empty(),
        "ByteReader: zero-byte read is valid at end");

    checkThrowsWithMessage(
        [&reader]() { (void)reader.readBytes(1); },
        "ByteReader: read past end",
        "ByteReader: read past end reports the exact failure");
    check(reader.position() == bytes.size(),
        "ByteReader: failed read leaves the position unchanged");

    reader.seek(0);
    check(reader.position() == 0 && reader.remaining() == bytes.size(),
        "ByteReader: seek to beginning restores the full span");
    reader.seek(bytes.size());
    checkThrowsWithMessage(
        [&reader, &bytes]() { reader.seek(bytes.size() + 1); },
        "ByteReader: seek past end",
        "ByteReader: seek past end reports the exact failure");
    check(reader.position() == bytes.size(),
        "ByteReader: failed seek leaves the position unchanged");

    const std::array signedBytes{
        std::byte{ 0xff },
        std::byte{ 0xff },
        std::byte{ 0xff },
        std::byte{ 0xff },
    };
    ByteReader signedReader{ std::span<const std::byte>(signedBytes) };
    check(signedReader.readLE<std::int32_t>() == -1,
        "ByteReader: signed little-endian values preserve two's-complement bits");

    ByteReader emptyReader(std::span<const std::byte>{});
    check(emptyReader.position() == 0
            && emptyReader.remaining() == 0
            && emptyReader.readString(0).empty(),
        "ByteReader: empty input supports exact zero-length reads");
}

} // namespace sed::tests
