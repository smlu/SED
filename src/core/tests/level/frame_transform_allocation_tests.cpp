#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/transform.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <optional>
#include <string_view>

namespace
{
thread_local std::optional<std::size_t> allocationsBeforeFailure;
thread_local bool allocationFailureFired = false;

void* allocate(std::size_t bytes)
{
    if ( allocationsBeforeFailure )
    {
        if ( *allocationsBeforeFailure == 0 )
        {
            allocationsBeforeFailure.reset();
            allocationFailureFired = true;
            throw std::bad_alloc();
        }
        --*allocationsBeforeFailure;
    }
    if ( void* memory = std::malloc(bytes == 0 ? 1 : bytes) )
    {
        return memory;
    }
    throw std::bad_alloc();
}
}

void* operator new(std::size_t bytes) { return allocate(bytes); }
void* operator new[](std::size_t bytes) { return allocate(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
using sed::tests::check;

sed::Level fixture(int mode)
{
    sed::Level level;
    level.layers = {"Frame allocation layer"};
    level.sectors.push_back(sed::tests::makeLitTestCubeSector(0.0, {}));
    for ( int index = 0; index < 3; ++index )
    {
        auto& thing = level.newThing();
        thing.name = "frame_allocation_owner_" + std::to_string(index);
        thing.x = thing.y = thing.z = 0.25 * (index + 1);
        thing.sector = 0;
        sed::TplValue frame;
        frame.type = sed::ValueType::frame;
        frame.name = "frame";
        frame.stringValue = index == 1 ? "(-2.5/0.25/1.5:7.25/8.5/9.75)"
                                      : "(1.25/2.5/3.75:4.5/5.25/6.125)";
        if ( mode == 2 )
        {
            // Rotation-axis points have exact fixed coordinates while parsing,
            // allocation and canonical serialization still execute fully.
            frame.stringValue = index == 1 ? "0 0 -1.5 7.25 8.5 9.75"
                                          : "0 0 3.75 4.5 5.25 6.125";
        }
        thing.values.push_back(frame);
        frame.stringValue = "(malformed frame retained verbatim)";
        thing.values.push_back(frame);
        frame.type = sed::ValueType::str;
        frame.name = "unrelated";
        frame.stringValue = "untouched authored field";
        thing.values.push_back(frame);
    }
    return level;
}

bool transform(sed::Level& level, int mode)
{
    // Repeated references retain legacy sequential application; invalid and
    // non-frame references are skipped without disturbing other fields.
    const std::array<sed::level::FrameRef, 7> frames{{{0, 0}, {1, 0}, {0, 0},
        {-1, 0}, {99, 0}, {0, 1}, {0, 2}}};
    switch ( mode )
    {
    case 0:
    {
        const std::array<std::int32_t, 5> things{{0, 1, 0, -1, 99}};
        return sed::level::levelTranslateThings(level, things, 1, -2, 3, true, true);
    }
    case 1:
        return sed::level::levelTranslateFrames(level, frames, 1, -2, 3);
    case 2:
        return sed::level::levelRotateFrames(level, frames, {0, 0, 1}, 0, 0, 0, 90, true);
    case 3:
    {
        sed::ScaleData scale;
        scale.sfactor = 2;
        return sed::level::levelScaleFrames(level, frames, scale);
    }
    case 4:
        return sed::level::levelFlipFramesOverPlane(level, frames, {1, 0, 0}, 0, 0, 0);
    }
    return false;
}

sed::Level expectedResult(const sed::Level& source, int mode)
{
    sed::Level expected = source;
    constexpr std::array<std::array<const char*, 2>, 5> tokens{{
        {{"(3.25/-1.5/9.75:4.5/5.25/6.125)", "(-1.5/-1.75/4.5:7.25/8.5/9.75)"}},
        {{"(3.25/-1.5/9.75:4.5/5.25/6.125)", "(-1.5/-1.75/4.5:7.25/8.5/9.75)"}},
        // rotatePYR stores the signed principal angle: 5.25 - 2*90,
        // and 8.5 - 90, respectively.
        {{"(0/0/3.75:4.5/-174.75/6.125)", "(0/0/-1.5:7.25/-81.5/9.75)"}},
        {{"(5/10/15:4.5/5.25/6.125)", "(-5/0.5/3:7.25/8.5/9.75)"}},
        {{"(1.25/2.5/3.75:4.5/5.25/6.125)", "(2.5/0.25/1.5:7.25/8.5/9.75)"}}
    }};
    expected.things[0].values[0].stringValue = tokens[static_cast<std::size_t>(mode)][0];
    expected.things[1].values[0].stringValue = tokens[static_cast<std::size_t>(mode)][1];
    if ( mode == 0 )
    {
        expected.things[0].x = 2.25;
        expected.things[0].y = -3.75;
        expected.things[0].z = 6.25;
        expected.things[1].x = 1.5;
        expected.things[1].y = -1.5;
        expected.things[1].z = 3.5;
        expected.things[0].sector = expected.things[1].sector = -1;
    }
    return expected;
}

void checkUnchangedReturns()
{
    sed::Level level = fixture(2);
    const auto before = sed::assets::saveToSedBytes(level);
    const std::array<sed::level::FrameRef, 1> selected{{{0, 0}}};
    const std::array<sed::level::FrameRef, 3> invalid{{{-1, 0}, {0, 1}, {0, 2}}};
    const std::array<std::int32_t, 1> things{{0}};
    const std::array<std::int32_t, 2> invalidThings{{-1, 99}};
    sed::ScaleData identity;
    check(!sed::level::levelTranslateThings(level, things, 0, 0, 0, true)
            && !sed::level::levelTranslateFrames(level, selected, 0, 0, 0)
            && !sed::level::levelRotateFrames(level, selected, {0, 0, 1}, 0, 0, 0, 0, true)
            && !sed::level::levelScaleFrames(level, selected, identity)
            && !sed::level::levelFlipFramesOverPlane(level, selected, {1, 0, 0}, 0, 0, 0),
        "Frame transform no-ops return false without canonicalizing authored tokens");
    check(!sed::level::levelTranslateThings(level, invalidThings, 1, 2, 3, true)
            && !sed::level::levelTranslateFrames(level, invalid, 1, 2, 3)
            && !sed::level::levelRotateFrames(level, invalid, {0, 0, 1}, 0, 0, 0, 90, true)
            && !sed::level::levelScaleFrames(level, invalid, sed::ScaleData{0, 0, 0, 2})
            && !sed::level::levelFlipFramesOverPlane(level, invalid, {1, 0, 0}, 0, 0, 0),
        "Frame transform all-invalid selections return false");
    check(sed::assets::saveToSedBytes(level) == before,
        "Frame transform false no-op and invalid results preserve complete original Level");
    level.things[0].values[0].stringValue = "1.25 2.5 3.75 4.5 5.25 6.125";
    const auto duplicateBefore = sed::assets::saveToSedBytes(level);
    const std::array<sed::level::FrameRef, 2> repeated{{{0, 0}, {0, 0}}};
    check(!sed::level::levelFlipFramesOverPlane(level, repeated, {1, 0, 0}, 0, 0, 0)
            && sed::assets::saveToSedBytes(level) == duplicateBefore,
        "Frame transform duplicate cancellation returns false and preserves exact original token spelling");
    level.things[0].sector = -1;
    sed::Level expectedMembership = level;
    expectedMembership.things[0].sector = 0;
    check(sed::level::levelTranslateThings(level, things, 0, 0, 0, true, true)
            && sed::assets::saveToSedBytes(level) == sed::assets::saveToSedBytes(expectedMembership),
        "Thing zero translation with requested containment repair returns true and changes only the sector index");
    level.things[0].x = -0.0;
    const auto signedZeroBefore = sed::assets::saveToSedBytes(level);
    check(!sed::level::levelTranslateThings(level, things, 0, 0, 0, false, true)
            && std::signbit(level.things[0].x)
            && sed::assets::saveToSedBytes(level) == signedZeroBefore,
        "Thing zero translation returning false preserves authored negative-zero coordinate bits");
}

void checkArithmeticRejection(int mode)
{
    sed::Level level = fixture(mode);
    level.things[1].values[0].stringValue = "(1e+308/2/3:4/5/6)";
    const auto before = sed::assets::saveToSedBytes(level);
    const std::array<sed::level::FrameRef, 2> frames{{{0, 0}, {1, 0}}};
    bool changed = false;
    switch ( mode )
    {
    case 0:
    {
        const std::array<std::int32_t, 2> things{{0, 1}};
        changed = sed::level::levelTranslateThings(level, things, 1e308, 0, 0, true, true);
        break;
    }
    case 1:
        changed = sed::level::levelTranslateFrames(level, frames, 1e308, 0, 0);
        break;
    case 2:
        changed = sed::level::levelRotateFrames(level, frames, {0, 0, 1}, -1e308, 0, 0, 90, false);
        break;
    case 3:
    {
        sed::ScaleData scale;
        scale.sfactor = 2;
        changed = sed::level::levelScaleFrames(level, frames, scale);
        break;
    }
    case 4:
        changed = sed::level::levelFlipFramesOverPlane(level, frames, {1, 0, 0}, 0, 0, 0);
        break;
    }
    bool unchanged = false;
    try
    {
        unchanged = sed::assets::saveToSedBytes(level) == before;
    }
    catch ( const std::exception& )
    {
        // An invalid generated Thing pose may itself be rejected by the native
        // writer; that is still a failed atomic transform assertion.
    }
    check(!changed && unchanged, "Frame transform arithmetic overflow rejects the whole batch before publishing tokens or poses");
}
}

int main(int argc, char** argv)
{
    constexpr std::array<std::string_view, 5> modes{{"translate-things", "translate-frames",
        "rotate-frames", "scale-frames", "flip-frames"}};
    if ( argc != 2 )
    {
        return 2;
    }
    const auto found = std::find(modes.begin(), modes.end(), std::string_view(argv[1]));
    if ( found == modes.end() )
    {
        return 2;
    }
    const int mode = static_cast<int>(found - modes.begin());
    checkUnchangedReturns();
    checkArithmeticRejection(mode);
    std::set_terminate([]
    {
        std::fputs("Frame transform terminated instead of propagating allocation failure\n", stderr);
        std::_Exit(86);
    });
    bool reachedSuccess = false;
    std::size_t rejected = 0;
    for ( std::size_t failurePosition = 0; failurePosition < 2048; ++failurePosition )
    {
        sed::Level level = fixture(mode);
        const auto before = sed::assets::saveToSedBytes(level);
        const auto expected = expectedResult(level, mode);
        const auto expectedBytes = sed::assets::saveToSedBytes(expected);
        bool caught = false;
        bool changed = false;
        allocationFailureFired = false;
        allocationsBeforeFailure = failurePosition;
        try
        {
            changed = transform(level, mode);
        }
        catch ( const std::bad_alloc& )
        {
            caught = true;
        }
        allocationsBeforeFailure.reset();
        check(caught == allocationFailureFired,
            "Frame transform propagates every injected allocation failure instead of silently skipping a parsed value");
        if ( allocationFailureFired )
        {
            ++rejected;
            check(sed::assets::saveToSedBytes(level) == before,
                "Frame transform allocation failure preserves complete level before any publication");
        }
        else
        {
            if ( level.things[0].values[0].stringValue != expected.things[0].values[0].stringValue
                || level.things[1].values[0].stringValue != expected.things[1].values[0].stringValue )
            {
                std::fprintf(stderr, "Frame mode %d success actual=[%s] [%s] expected=[%s] [%s]\n", mode,
                    level.things[0].values[0].stringValue.c_str(), level.things[1].values[0].stringValue.c_str(),
                    expected.things[0].values[0].stringValue.c_str(), expected.things[1].values[0].stringValue.c_str());
            }
            check(changed, "Frame transform success returns true for actual authored changes");
            check(level.things[0].values[0].stringValue == expected.things[0].values[0].stringValue
                    && level.things[1].values[0].stringValue == expected.things[1].values[0].stringValue,
                "Frame transform success matches both complete fixed authored tokens");
            check(sed::assets::saveToSedBytes(level) == expectedBytes,
                "Frame transform success changes only fixed selected values and requested Thing poses");
            reachedSuccess = true;
            break;
        }
    }
    check(rejected > 0 && reachedSuccess,
        "Frame transform allocation sweep covers every allocation failure through complete success");
    return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
