#include "test_common.hpp"
#include "ui/audio/audio_resource_identity_utils.hpp"
#include <QTemporaryDir>
#include <chrono>
#include <fstream>

namespace sed::app_tests
{
void runAudioResourceIdentityTests()
{
    using namespace ui::audio;
    const std::vector<std::byte> firstBytes{std::byte{1}, std::byte{2}, std::byte{3}};
    const std::vector<std::byte> replacement{std::byte{1}, std::byte{7}, std::byte{3}};
    const auto firstKey = soundBytesRequestKey("same.wav", firstBytes);
    // Independently generated SHA-256 vectors: each field is preceded by
    // its little-endian u64 byte length. Never derive expectations with the
    // production key helper or Qt's hash implementation under test.
    check(soundBytesRequestKey("same.wav", firstBytes)
            == "BYTES:042a3ea919abf0e0a47d4a8ba53a8ba9d5faca569440462477d3e995db34724d",
        "Audio full byte identity matches independent length-framed SHA-256 vector");
    check(soundBytesRequestKey("same.wav", replacement)
            == "BYTES:c26d537d1f5c7c2daeeaee49a79c7babdaad6eea02d47f7e3f2f425bb163bb51"
            && soundBytesRequestKey("other.wav", firstBytes)
            == "BYTES:7cd6780434ec9506df1453b6e2333013b982a90a69392b1f1c590433fd81152a",
        "Audio changed content and name each match complete fixed identity vectors");
    check(soundBytesRequestKey("", {})
            == "BYTES:fb5b72b37c5d00a42cc04b708faa349427c15641ff6fe346813eb7a3747a02bd",
        "Audio empty name and bytes match complete fixed identity vector");
    const std::vector<std::byte> binaryBytes{std::byte{0}, std::byte{255}};
    check(soundBytesRequestKey(std::string_view("a\0b", 3), binaryBytes)
            == "BYTES:341b0c87bf079beb5ebe6c1e48a0ea0e5a6b43368c3e22657845ba0522c35c0d",
        "Audio identity includes embedded NUL names and zero/high-bit bytes");
    assets::ResourceLocator emptyResources;
    check(soundResourceRequestKey(emptyResources, "absent.wav")
            == "RESOURCE:0da753714dc8a948d84823928a5846922d4b250d82978127700d6c8460c5db1a",
        "Audio missing-resource identity matches complete fixed revision vector");
    check(firstKey == soundBytesRequestKey("same.wav", firstBytes),
        "Audio unchanged byte request retains active-owner/decoded-cache identity");
    check(firstKey != soundBytesRequestKey("same.wav", replacement),
        "Audio same-owner/name changed bytes cannot reuse active sound or decoded PCM");
    check(firstKey != soundBytesRequestKey("other.wav", firstBytes),
        "Audio explicit distinct logical names remain distinct");
    check(soundBytesRequestKey(std::string(8192, 'x'), firstBytes).size() <= 80,
        "Audio request/cache keys remain bounded for long logical names");
    QTemporaryDir firstRoot, secondRoot;
    check(firstRoot.isValid() && secondRoot.isValid(), "Audio resource revision fixtures exist");
    if (!firstRoot.isValid() || !secondRoot.isValid()) return;
    const auto firstPath = std::filesystem::path(firstRoot.path().toStdWString());
    const auto secondPath = std::filesystem::path(secondRoot.path().toStdWString());
    assets::ResourceLocator first, second;
    first.addRoot(firstPath);
    second.addRoot(secondPath);
    std::ofstream(firstPath / "same.wav", std::ios::binary) << "first";
    std::ofstream(secondPath / "same.wav", std::ios::binary) << "second";
    const auto diskKey = soundResourceRequestKey(first, "same.wav");
    check(diskKey != soundResourceRequestKey(second, "same.wav"),
        "Audio resource key distinguishes project source roots");
    std::ofstream(firstPath / "same.wav", std::ios::binary) << "replacement";
    std::filesystem::last_write_time(firstPath / "same.wav",
        std::filesystem::file_time_type::clock::now() + std::chrono::seconds(10));
    check(diskKey != soundResourceRequestKey(first, "same.wav"),
        "Audio resource key detects replacement before active-owner early return");
    const auto missing = soundResourceRequestKey(first, "created.wav");
    std::ofstream(firstPath / "created.wav", std::ios::binary) << "created";
    check(missing != soundResourceRequestKey(first, "created.wav"),
        "Audio missing-resource identity changes when sound becomes available");
    assets::ResourceLocator exactBytes;
    exactBytes.setPrimaryResourceOverride("bytes.wav", [binaryBytes] { return std::optional(binaryBytes); });
    check(readSoundResource(exactBytes, "bytes.wav") == std::optional(binaryBytes),
        "Audio resource read success returns every fixed binary byte unchanged");
    exactBytes.setPrimaryResourceOverride("bytes.wav", []
        { return std::optional(std::vector<std::byte>{}); });
    const auto zeroBytes = readSoundResource(exactBytes, "bytes.wav");
    check(zeroBytes && zeroBytes->empty() && !readSoundResource(exactBytes, "missing.wav"),
        "Audio resource read distinguishes present empty bytes from unavailable name");
    assets::ResourceLocator unreadable;
    unreadable.setPrimaryResourceOverride("unreadable.wav", []()
        -> std::optional<std::vector<std::byte>>
        { throw std::runtime_error("deterministic truncated resource read"); });
    bool threw = false;
    std::optional<std::vector<std::byte>> unavailable;
    try { unavailable = readSoundResource(unreadable, "unreadable.wav"); }
    catch (const std::exception&) { threw = true; }
    check(!threw && !unavailable,
        "Audio unreadable resource becomes unavailable through existing owner-safe failure path");

}
}
