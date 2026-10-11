#include "ui/viewport/viewport_orbit_utils.hpp"
#include "sed/assets/sed.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <optional>
#include <string_view>

// A separate single-threaded executable keeps allocator injection away from Qt
// event delivery. Only the const pivot call is armed; fixture/assertion work is not.
namespace
{
thread_local std::optional<std::size_t> allocationsBeforeFailure;
thread_local bool allocationFailureFired;
void* allocate(std::size_t size)
{
    if (allocationsBeforeFailure)
    {
        if (*allocationsBeforeFailure == 0)
        {
            allocationsBeforeFailure.reset();
            allocationFailureFired = true;
            throw std::bad_alloc();
        }
        --*allocationsBeforeFailure;
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}
int failures;
void check(bool condition, const char* message)
{
    if (!condition) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); }
}
}
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory,std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory,std::size_t) noexcept { std::free(memory); }

int main(int argc,char** argv)
{
    using sed::ui::viewport::selectionOrbitPivot;
    using sed::ui::viewport::SelectionOrbitFallback;
    const std::string_view mode = argc > 1 ? argv[1] : "";
    const bool uiBoundary = mode == "--ui-selected" || mode == "--ui-fallback";
    const bool fallbackMode = mode == "--fallback" || mode == "--ui-fallback";
    std::set_terminate([]
    {
        std::fputs("Orbit Frame allocation terminated instead of propagating failure\n",stderr);
        std::_Exit(86);
    });
    sed::Level level;
    level.layers = {"Layer0"};
    auto& owner = level.things.emplace_back();
    owner.x = 40; owner.y = 50; owner.z = 60;
    sed::TplValue frame;
    frame.name = "frame";
    frame.type = sed::ValueType::frame;
    frame.stringValue = "(1.125/2.25/3.5:4.75/5.875/6.0625)";
    owner.values.push_back(frame);
    sed::editor::Selection selection;
    SelectionOrbitFallback fallback;
    if (fallbackMode) { fallback.thing = 0; fallback.frameValue = 0; }
    else { selection.things.insert(0); selection.frames.insert({0,0}); }
    const auto before = sed::assets::saveToSedBytes(level);
    const auto baseline = selectionOrbitPivot(level,selection,fallback);
    check(baseline && *baseline == QVector3D(1.125f,2.25f,3.5f),
        "selected and active-fallback Frame pivot return fixed authored position without averaging owner");
    bool reachedSuccess = false;
    std::size_t rejected = 0;
    for (std::size_t failAfter = 0; failAfter < 128 && !reachedSuccess; ++failAfter)
    {
        std::optional<QVector3D> result = QVector3D(11,22,33);
        bool caught = false;
        bool completed = false;
        allocationFailureFired = false;
        allocationsBeforeFailure = failAfter;
        try
        {
            if (uiBoundary)
                completed = sed::ui::viewport::trySelectionOrbitPivot(level,selection,fallback,result);
            else
            {
                result = selectionOrbitPivot(level,selection,fallback);
                completed = true;
            }
        }
        catch (const std::bad_alloc&) { caught = true; }
        allocationsBeforeFailure.reset();
        check(!uiBoundary || !caught,"UI orbit boundary reports allocation failure as false instead of throwing through Qt");
        check(!allocationFailureFired || (uiBoundary ? (!caught && !completed) : caught),
            "every fired allocation must be reported as UI false or propagated pure-helper failure");
        check(allocationFailureFired || (!caught && completed),
            "unarmed successful schedule must report completion");
        if (allocationFailureFired || caught || !completed)
        {
            ++rejected;
            check(result && *result == QVector3D(11,22,33),
                "orbit allocation failure preserves caller result sentinel instead of publishing partial pivot");
        }
        else
        {
            reachedSuccess = true;
            check(result && *result == QVector3D(1.125f,2.25f,3.5f),
                "orbit allocation sweep reaches complete fixed-value success");
        }
        check(sed::assets::saveToSedBytes(level) == before,
            "orbit allocation failure and success preserve complete input Level");
        check(fallbackMode ? (selection.frames.empty() && selection.things.empty())
                           : (selection.frames.size() == 1 && selection.frames.contains({0,0})
                               && selection.things.size() == 1 && selection.things.contains(0)),
            "orbit allocation sweep preserves input semantic selection");
    }
    check(rejected > 0 && reachedSuccess,"orbit allocation sweep exercises failure and complete success");
    check(!selectionOrbitPivot(level,sed::editor::Selection{}),"empty selection has no orbit pivot");
    std::optional<QVector3D> emptyResult = QVector3D(11,22,33);
    check(sed::ui::viewport::trySelectionOrbitPivot(level,{}, {},emptyResult) && !emptyResult,
        "UI orbit boundary distinguishes successful absent pivot from failure and replaces OUT sentinel");
    std::optional<QVector3D> validResult = QVector3D(11,22,33);
    check(sed::ui::viewport::trySelectionOrbitPivot(level,selection,fallback,validResult)
            && validResult && *validResult == QVector3D(1.125f,2.25f,3.5f),
        "UI orbit boundary publishes complete fixed pivot with true RETURN");
    level.things[0].values[0].stringValue = "(1/2/3:4/5/invalid)";
    check(!selectionOrbitPivot(level,selection,fallback),"malformed Frame contributes no partial orbit pivot");
    std::optional<QVector3D> malformedResult = QVector3D(11,22,33);
    check(sed::ui::viewport::trySelectionOrbitPivot(level,selection,fallback,malformedResult) && !malformedResult,
        "UI orbit boundary treats malformed Frame as successful absent pivot and replaces OUT sentinel");
    check(!selectionOrbitPivot(level,{},SelectionOrbitFallback{.thing=0,.frameValue=999}),
        "invalid active Frame reference contributes no orbit pivot");
    std::fprintf(stderr,"Orbit allocation rejected=%zu success=%d failures=%d\n",rejected,reachedSuccess,failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
