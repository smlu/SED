#include "test_common.hpp"
#include "../assets/resource_locator_test_fixtures.hpp"

#include "sed/assets/sed.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/assets/pup.hpp"
#include <sstream>
#include "sed/editor/document.hpp"

#include <cstdlib>
#include <cstdio>
#include <exception>
#include "sed/parse_frame.hpp"
#include "sed/parse.hpp"
#include <limits>
#include <new>
#include <optional>

// This executable is deliberately separate from sed_core_tests: allocator
// injection is local to its sole thread and armed only around one transition.
// Sweeping all allocation positions avoids depending on allocator sizes or STL
// implementation details. Assertions/serialization run after injection is off.
namespace
{
thread_local std::optional<std::size_t> allocationsBeforeFailure{};
thread_local std::size_t* allocationCounter{};

void* allocate(const std::size_t bytes)
{
    if ( allocationCounter ) ++*allocationCounter;
    if ( allocationsBeforeFailure )
    {
        if ( *allocationsBeforeFailure == 0 )
        {
            allocationsBeforeFailure.reset();
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

void* operator new(const std::size_t bytes) { return allocate(bytes); }
void* operator new[](const std::size_t bytes) { return allocate(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
using sed::editor::DocumentUndoHistory;
using sed::editor::DocumentDirty;
using sed::editor::Selection;
using sed::tests::check;

bool sameSelection(const Selection& a, const Selection& b)
{
    const auto sameSet = [](const auto& left, const auto& right)
    {
        return left.size() == right.size()
            && std::all_of(left.begin(), left.end(),
                [&](const auto& value) { return right.contains(value); });
    };
    return sameSet(a.sectors, b.sectors) && sameSet(a.surfaces, b.surfaces)
        && sameSet(a.surfaceVertices, b.surfaceVertices)
        && sameSet(a.things, b.things) && sameSet(a.frames, b.frames)
        && sameSet(a.lights, b.lights) && sameSet(a.cogs, b.cogs);
}

bool sameState(const sed::editor::DocumentState& a,
    const sed::editor::DocumentState& b)
{
    return sameSelection(a.selection, b.selection)
        && sed::assets::saveToSedBytes(a.level, {},
            sed::assets::SedSerializationProfile::UnprotectedNested)
        == sed::assets::saveToSedBytes(b.level, {},
            sed::assets::SedSerializationProfile::UnprotectedNested);
}

bool sameHistory(const DocumentUndoHistory& a, const DocumentUndoHistory& b)
{
    if ( a.undoDepth() != b.undoDepth() || a.redoDepth() != b.redoDepth()
        || a.undoSignature() != b.undoSignature()
        || a.redoSignature() != b.redoSignature()
        || !sameState(a.current(), b.current()) )
    {
        return false;
    }
    const auto left = a.recoveryHistory();
    const auto right = b.recoveryHistory();
    const auto sameEntries = [](const auto& entries, const auto& expected)
    {
        if ( entries.size() != expected.size() ) return false;
        for ( std::size_t index = 0; index < entries.size(); ++index )
        {
            const auto& x = entries[index];
            const auto& y = expected[index];
            if ( x.label != y.label || x.dirtyFlags != y.dirtyFlags
                || !sameState(x.state, y.state)
                || x.navigationSelection.has_value() != y.navigationSelection.has_value()
                || (x.navigationSelection
                    && !sameSelection(*x.navigationSelection, *y.navigationSelection)) )
            {
                return false;
            }
        }
        return true;
    };
    return sameEntries(left.undoEntries, right.undoEntries)
        && sameEntries(left.redoEntries, right.redoEntries);
}

void pushRecord(DocumentUndoHistory& history, const int variant)
{
    const auto things = static_cast<std::uint32_t>(DocumentDirty::Things);
    const auto geometry = static_cast<std::uint32_t>(DocumentDirty::LevelGeometry);
    switch ( variant )
    {
    case 0: history.pushUndo("Full snapshot with a long label", 0); break;
    case 1: history.pushUndo("Things", things); break;
    case 2: history.pushUndo("Lights", static_cast<std::uint32_t>(DocumentDirty::Lights)); break;
    case 3: history.pushUndo("Cogs", static_cast<std::uint32_t>(DocumentDirty::Cogs)); break;
    case 4: history.pushUndo("Things and lights", things | static_cast<std::uint32_t>(DocumentDirty::Lights)); break;
    case 5: history.pushThingSubsetUndo("Subset Things", things, {0, 2}); break;
    case 6: history.pushSectorSubsetUndo("Subset Sectors", geometry, {0}); break;
    case 7: history.pushSurfaceSubsetUndo("Subset Surfaces", geometry, {0}); break;
    case 8: history.pushThingTailUndo("Append Things", things); break;
    case 9: history.appendCompactRecoveryState(
        sed::editor::DocumentRecoveryHistoryStack::Undo, "Imported history row", 0,
        history.current().level, history.current().selection); break;
    }
}

DocumentUndoHistory fixture(const int variant, const bool redo)
{
    DocumentUndoHistory history{};
    auto& state = history.current();
    state.level.sectors.push_back(sed::tests::makeLitTestCubeSector(0.0, {}));
    for ( int index = 0; index < 3; ++index )
    {
        state.level.newThing().name = std::string(80, static_cast<char>('a' + index));
    }
    state.level.lights.emplace_back();
    state.level.cogs.emplace_back();
    state.selection.sectors.insert(0);
    state.selection.surfaces.insert({0, 0});
    state.selection.surfaceVertices.insert({{0, 0}, 0});
    state.selection.things.insert(0);
    state.selection.frames.insert({0, 0});
    state.selection.lights.insert(0);
    state.selection.cogs.insert(0);
    history.pushUndo("Earlier full operation");
    state.level.ppunit = 321.0;
    pushRecord(history, variant);
    if ( variant == 0 ) state.level.ppunit = 42.0;
    if ( variant == 1 || variant == 4 || variant == 5 ) state.level.things[0].x = 42.0;
    if ( variant == 2 || variant == 4 )
    {
        state.level.lights[0].position.x = 42.0;
        state.level.sectors[0].surfaces[0].texVertices[0].color.r = 0.75f;
    }
    if ( variant == 3 ) state.level.cogs[0].name = "edited.cog";
    if ( variant == 6 || variant == 7 ) state.level.sectors[0].vertices[0].x = -2.0;
    if ( variant == 8 )
    {
        for ( int index = 0; index < 8; ++index )
            state.level.newThing().name = std::string(80, 'z');
    }
    state.level.curId += 17;
    state.selection.clear();
    state.selection.things.insert(2);
    if ( redo )
    {
        check(history.undo(), "allocation fixture: establish redo state");
        // Force the append redo path to grow its destination vector.
        state.level.things.shrink_to_fit();
    }
    else
    {
        // Both stacks must survive the failed undo, including an unrelated row.
        history.pushUndo("Later operation");
        state.level.ppunit += 1.0;
        check(history.undo(), "allocation fixture: preserve a nonempty redo stack");
    }
    return history;
}
}

int main(int argc, char** argv)
{
    // Deliberately isolated: the old false-noexcept contracts terminate on
    // injected allocation failure. Root invokes each mode as its own process.
    if (argc != 1 && argc != 2)
    {
        std::fputs("Expected no arguments or one allocation contract mode\n", stderr);
        return 2;
    }
    if (argc == 2)
    {
        const std::string_view mode(argv[1]);
        if (mode == "--pup-parse-fault")
        {
            std::set_terminate([]
            {
                std::fputs("PUP allocation contract terminated instead of propagating failure\n", stderr);
                std::_Exit(86);
            });
            const std::string name = "animation_name_long_enough_to_allocate_without_small_string_optimization.KeY";
            bool reachedSuccess = false;
            std::size_t rejected = 0;
            for (std::size_t position = 0; position < 256; ++position)
            {
                std::istringstream input("0 " + name + "\n");
                // Preserve the original allocation exception from streambuf or
                // getline rather than asking the loader to diagnose badbit.
                input.exceptions(std::ios::badbit);
                sed::assets::PupFile published;
                published.keys = {"prior.key"};
                bool caught = false;
                allocationsBeforeFailure = position;
                try { published = sed::assets::loadFromPUP(input); }
                catch (const std::bad_alloc&) { caught = true; }
                allocationsBeforeFailure.reset();
                if (caught)
                {
                    ++rejected;
                    check(published.keys == std::vector<std::string>{"prior.key"},
                        "PUP allocation failure preserves caller's previous complete result");
                }
                else
                {
                    check(published.keys == std::vector<std::string>{name},
                        "PUP allocation sweep completes with exact authored mixed-case KEY filename");
                    reachedSuccess = true;
                    break;
                }
            }
            check(rejected > 0 && reachedSuccess, "PUP allocation sweep covers failure and complete success");
            return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (mode != "--template-find-fault" && mode != "--template-field-fault"
            && mode != "--frame-parse-fault" && mode != "--frame-get-fault"
            && mode != "--frame-default-get-fault" && mode != "--surface-ref-fault")
        {
            std::fputs("Unknown allocation contract mode\n", stderr);
            return 2;
        }
        sed::assets::Templates templates;
        sed::ThingTemplate item;
        item.name = "allocation_contract_template_with_long_name";
        item.parent = "none";
        sed::TplValue value;
        value.name = "frame";
        value.type = sed::ValueType::frame;
        value.stringValue = "(1.125/2.25/3.5:4.75/5.875/6.0625)";
        item.values.push_back(value);
        templates.addOrUpdate(item);
        const std::string longSurfaceRef = "12" + std::string(64, ' ') + "34";
        double x = 11, y = 22, z = 33, pch = 44, yaw = 55, rol = 66;
        std::set_terminate([]
        {
            std::fputs("Allocation contract terminated instead of propagating failure\n", stderr);
            std::_Exit(86);
        });
        bool validSuccess = false;
        bool caught = false;
        allocationsBeforeFailure = 0;
        try
        {
            if (mode == "--template-find-fault")
                validSuccess = templates.find(item.name) == templates.get(0);
            else if (mode == "--template-field-fault")
                validSuccess = templates.findField(item.name, "frame") == &templates.get(0)->values[0];
            else if (mode == "--frame-parse-fault")
            {
                const auto parsed = sed::parse::frameValue(value.stringValue);
                validSuccess = parsed && parsed->x == 1.125 && parsed->y == 2.25 && parsed->z == 3.5
                    && parsed->pch == 4.75 && parsed->yaw == 5.875 && parsed->rol == 6.0625;
            }
            else if (mode == "--frame-get-fault")
                validSuccess = value.tryGetFrame(x, y, z, pch, yaw, rol)
                    && x == 1.125 && y == 2.25 && z == 3.5 && pch == 4.75 && yaw == 5.875 && rol == 6.0625;
            else if (mode == "--frame-default-get-fault")
            {
                value.getFrame(x, y, z, pch, yaw, rol);
                validSuccess = x == 1.125 && y == 2.25 && z == 3.5 && pch == 4.75 && yaw == 5.875 && rol == 6.0625;
            }
            else if (mode == "--surface-ref-fault")
            {
                const auto parsed = sed::parse::surfaceRef(longSurfaceRef);
                validSuccess = parsed && parsed->sector == 12 && parsed->surface == 34;
            }
        }
        catch (const std::bad_alloc&) { caught = true; }
        allocationsBeforeFailure.reset();
        check(caught || validSuccess, "Allocation contract either propagates failure or completes without allocating");
        if (caught)
            check(x == 11 && y == 22 && z == 33 && pch == 44 && yaw == 55 && rol == 66,
                "Failed Frame allocation preserves all six OUT arguments");
        return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    {
        const std::vector<std::int32_t> selected{0, 1};
        bool reachedSuccess = false;
        std::size_t rejected = 0;
        for (std::size_t position = 0; position < 256; ++position)
        {
            auto history = fixture(0, false);
            history.current().level.things.resize(3);
            history.current().level.things[0].x = 11;
            history.current().level.things[1].name = "original second thing";
            history.current().level.things[2].name = "untouched third thing";
            const auto before = history;
            bool result = false;
            bool failed = false;
            allocationsBeforeFailure = position;
            try
            {
                result = history.applyThingSubset("subset command", selected,
                    [](sed::editor::DocumentState& state)
                    {
                        state.level.things[0].x = 22;
                        state.level.things[1].name.assign(200, 'n');
                        state.selection.things.clear();
                        state.selection.things.insert(1);
                        return true;
                    });
            }
            catch (const std::bad_alloc&) { failed = true; }
            allocationsBeforeFailure.reset();
            if (failed)
            {
                ++rejected;
                check(!result && sameHistory(history, before),
                    "Subset command allocation rejection preserves full current state, both stacks and IDs");
            }
            else
            {
                check(result && history.current().level.things[0].x == 22
                        && history.current().level.things[1].name == std::string(200, 'n')
                        && history.current().level.things[2].name == "untouched third thing"
                        && history.current().selection.things.size() == 1
                        && history.current().selection.things.contains(1)
                        && history.nextUndoLabel() == "subset command"
                        && history.undoDepth() == before.undoDepth() + 1 && history.redoDepth() == 0,
                    "Subset command publishes exact chosen state and one named history row");
                const auto committed = history.current();
                std::uint32_t dirty{};
                std::vector<std::int32_t> affected;
                check(history.undo(&dirty, &affected)
                        && dirty == static_cast<std::uint32_t>(DocumentDirty::Things)
                        && affected == selected && sameState(history.current(), before.current()),
                    "Subset command retains compact selected-Thing Undo and exact previous state");
                dirty = static_cast<std::uint32_t>(DocumentDirty::All);
                affected = {-99};
                check(history.redo(&dirty, &affected)
                        && dirty == static_cast<std::uint32_t>(DocumentDirty::Things)
                        && affected == selected && sameState(history.current(), committed)
                        && history.nextUndoLabel() == "subset command",
                    "Subset command Redo publishes exact committed state, label, dirty mask and affected indices");
                reachedSuccess = true;
                break;
            }
        }
        check(rejected > 0 && reachedSuccess, "Subset command allocation sweep reaches failures and success");
        auto noOp = fixture(0, false);
        noOp.current().level.things.resize(3);
        const auto beforeNoOp = noOp;
        const bool applied = noOp.applyThingSubset("rejected subset", selected,
            [](sed::editor::DocumentState& state)
            {
                state.level.things[0].x = 999;
                state.selection.clear();
                return false;
            });
        check(!applied && sameHistory(noOp, beforeNoOp),
            "Rejected subset command restores selected values and selection without changing either history stack or IDs");
    }
    {
        bool reachedSuccess = false;
        std::size_t rejected = 0;
        for (std::size_t position = 0; position < 128; ++position)
        {
            sed::assets::Templates templates;
            sed::ThingTemplate old;
            old.name = "existing_template_with_non_small_name";
            old.parent = "none";
            old.desc = "original description";
            templates.addOrUpdate(old);
            sed::ThingTemplate added;
            added.name = "new_template_with_non_small_name";
            added.parent = old.name;
            added.desc = "new description";
            std::size_t insertedIndex = 999;
            bool failed = false;
            allocationsBeforeFailure = position;
            try { insertedIndex = templates.addOrUpdate(std::move(added)); }
            catch (const std::bad_alloc&) { failed = true; }
            allocationsBeforeFailure.reset();
            const auto* original = templates.get(0);
            const bool originalUnchanged = original && original->name == old.name
                && original->parent == "none" && original->desc == "original description"
                && templates.find(old.name) == original;
            if (failed)
            {
                ++rejected;
                check(originalUnchanged && templates.size() == 1 && !templates.get(1)
                        && !templates.find("new_template_with_non_small_name"),
                    "Template insertion allocation failure preserves item vector and lookup index atomically");
            }
            else
            {
                const auto* inserted = templates.get(1);
                check(originalUnchanged && insertedIndex == 1 && templates.size() == 2
                        && inserted && inserted->name == "new_template_with_non_small_name"
                        && inserted->parent == old.name && inserted->desc == "new description"
                        && templates.find(inserted->name) == inserted,
                    "Template insertion success publishes exact return index, item and lookup together");
                reachedSuccess = true;
                break;
            }
        }
        check(reachedSuccess && rejected != 0,
            "Template insertion allocation sweep exercises rejection and eventual complete publication");
    }
    for (const bool update : {false, true})
    {
        bool reachedSuccess = false;
        std::size_t rejected = 0;
        for (std::size_t position = 0; position < 128; ++position)
        {
            sed::assets::Templates templates;
            sed::ThingTemplate original;
            original.name = "existing_template_with_non_small_name";
            original.parent = "none";
            original.desc = "original description";
            original.bbox.Set(1, 2, 3, 4, 5, 6);
            original.values.push_back(sed::deserializeTplValue("mass", "17.25"));
            templates.addOrUpdate(original);
            auto replacement = original;
            replacement.name = "EXISTING_TEMPLATE_WITH_NON_SMALL_NAME";
            replacement.parent = "changed-parent";
            replacement.desc = "replacement description";
            replacement.bbox.Set(-1, -2, -3, 7, 8, 9);
            replacement.values[0].stringValue = "91.5";
            const auto expected = update ? replacement : original;
            std::size_t index = 999;
            bool failed = false;
            allocationsBeforeFailure = position;
            try { index = templates.addOrUpdate(std::move(replacement), update); }
            catch (const std::bad_alloc&) { failed = true; }
            allocationsBeforeFailure.reset();
            const auto* actual = templates.get(0);
            const auto& target = failed ? original : expected;
            const bool exact = actual && actual->name == target.name && actual->parent == target.parent
                && actual->desc == target.desc && actual->bbox.x1 == target.bbox.x1
                && actual->bbox.y1 == target.bbox.y1 && actual->bbox.z1 == target.bbox.z1
                && actual->bbox.x2 == target.bbox.x2 && actual->bbox.y2 == target.bbox.y2
                && actual->bbox.z2 == target.bbox.z2 && actual->values.size() == 1
                && actual->values[0].name == "mass" && actual->values[0].type == sed::ValueType::str
                && actual->values[0].stringValue == target.values[0].stringValue
                && templates.size() == 1 && !templates.get(1) && templates.find(original.name) == actual;
            if (failed)
            {
                ++rejected;
                check(exact && index == 999, "Template update allocation failure preserves complete original item/index and caller result");
            }
            else
            {
                check(exact && index == 0, "Template update success or update-disabled no-op returns exact existing index and complete fields");
                reachedSuccess = true;
                break;
            }
        }
        check(reachedSuccess && rejected != 0, "Template update allocation sweep covers failure and success for both update policies");
    }
    // Raw DTOs, retained compact imports, and uniquely owned compact imports
    // must publish the complete replacement only after all allocations succeed.
    for (int mode = 0; mode < 3; ++mode)
    {
        bool reachedSuccess = false;
        std::size_t rejected = 0;
        for (std::size_t position = 0; position < 1024; ++position)
        {
            auto source = fixture(5, false);
            auto history = fixture(6, false);
            auto previous = history;
            auto desired = history;
            auto importedCurrent = source.current();
            auto recovery = mode == 0 ? source.recoveryHistory() : source.compactRecoveryHistory();
            auto expectedRecovery = mode == 0 ? source.recoveryHistory() : source.compactRecoveryHistory();
            desired.restoreRecoveryHistory(source.current(), std::move(expectedRecovery));
            const auto retained = mode == 1 ? recovery : sed::editor::DocumentRecoveryHistory{};
            const auto retainedBaseline = mode == 1 ? source : DocumentUndoHistory{};
            bool failed = false;
            allocationsBeforeFailure = position;
            try { history.restoreRecoveryHistory(std::move(importedCurrent), std::move(recovery)); }
            catch (const std::bad_alloc&) { failed = true; }
            allocationsBeforeFailure.reset();
            if (failed)
            {
                ++rejected;
                check(sameHistory(history, previous),
                    "Recovery import allocation failure preserves current state, both stacks and signatures");
                history.pushUndo("After rejected history import");
                previous.pushUndo("After rejected history import");
                check(history.undoSignature() == previous.undoSignature(),
                    "Recovery import allocation failure preserves next published record identity");
            }
            else
            {
                check(sameHistory(history, desired),
                    "Recovery import publishes exactly the complete requested state and history");
                reachedSuccess = true;
            }
            if (retained.compact)
                check(sameHistory(*retained.compact, retainedBaseline),
                    "Recovery import never changes a retained decoded compact history");
            if (reachedSuccess) break;
        }
        check(reachedSuccess && (mode == 2 || rejected != 0),
            "Recovery import allocation sweep reaches success after exercising fallible allocations");
        std::cout << "restore mode=" << mode << " rejected=" << rejected << '\n';
    }
    {
        sed::assets::ResourceLocator source;
        source.addRoot(std::filesystem::path(std::string(200, 'a')));
        source.addRoot(std::filesystem::path(std::string(200, 'b')));
        bool reachedSuccess = false;
        for ( std::size_t position = 0; position < 128; ++position )
        {
            sed::assets::ResourceLocator destination;
            destination.addRoot("old-first");
            destination.addRoot("old-second");
            const auto previousRoots = destination.roots();
            const auto previousIdentity = destination.previewCacheIdentity();
            bool failed = false;
            allocationsBeforeFailure = position;
            try { destination = source; }
            catch ( const std::bad_alloc& ) { failed = true; }
            allocationsBeforeFailure.reset();
            if ( failed )
            {
                check(destination.roots() == previousRoots && destination.gobs().empty()
                        && destination.previewCacheIdentity() == previousIdentity,
                    "ResourceLocator copy allocation failure preserves the complete prior configuration");
            }
            else
            {
                check(destination.roots() == source.roots()
                        && destination.previewCacheIdentity() == source.previewCacheIdentity(),
                    "ResourceLocator copy eventually publishes the complete new configuration");
                reachedSuccess = true;
                break;
            }
        }
        check(reachedSuccess, "ResourceLocator copy allocation sweep reaches success");
    }
    {
        sed::tests::ResourceLocatorFixture fixture;
        for (int mutation = 0; mutation < 6; ++mutation)
        {
            bool reachedSuccess = false;
            std::size_t rejected = 0;
            for (std::size_t position = 0; position < 512; ++position)
            {
                sed::assets::ResourceLocator original;
                original.addRoot(std::filesystem::path(std::string(200, 'r')));
                original.mountGob(fixture.firstGob);
                original.setPrimaryResourceOverride("pinned.spr", []
                    { return std::optional<std::vector<std::byte>>(std::vector<std::byte>{std::byte{'x'}}); });
                const auto retained = original;
                const auto* rootsOwner = &original.roots();
                const auto* gobsOwner = &original.gobs();
                const auto identity = original.previewCacheIdentity();
                bool failed = false;
                allocationsBeforeFailure = position;
                try
                {
                    switch (mutation)
                    {
                    case 0: original.addRoot("new-resource-root"); break;
                    case 1: original.clearRoots(); break;
                    case 2: original.mountGob(fixture.secondGob); break;
                    case 3: original.clearGobs(); break;
                    case 4: original.reloadGobs(); break;
                    case 5: original.setPrimaryResourceOverride("replacement.spr", []
                        { return std::optional<std::vector<std::byte>>{}; }); break;
                    }
                }
                catch (const std::exception&) { failed = true; }
                const bool injected = !allocationsBeforeFailure.has_value();
                allocationsBeforeFailure.reset();
                if (failed)
                {
                    ++rejected;
                    check(injected && &original.roots() == rootsOwner && &original.gobs() == gobsOwner
                            && original.previewCacheIdentity() == identity
                            && original.roots() == retained.roots()
                            && original.gobs().size() == retained.gobs().size()
                            && original.primaryResourceOverrideRevision("pinned.spr")
                                == retained.primaryResourceOverrideRevision("pinned.spr"),
                        "ResourceLocator failed COW/overlay mutation preserves exact owners, values, and identity");
                }
                else
                {
                    check(original.previewCacheIdentity() != identity,
                        "ResourceLocator successful COW/overlay mutation publishes new identity");
                    reachedSuccess = true;
                }
                check(retained.roots().size() == 1 && retained.gobs().size() == 1
                        && retained.previewCacheIdentity() == identity,
                    "ResourceLocator mutation leaves shared snapshot unchanged");
                if (reachedSuccess) break;
            }
            check(reachedSuccess && rejected != 0,
                "ResourceLocator mutation allocation sweep exercises failures and reaches success");
        }
    }
    {
        const auto cappedFixture = []
        {
            DocumentUndoHistory history{};
            history.current().level.newThing();
            history.current().selection.things.insert(0);
            // The first trim ensures spare vector capacity in both fixtures.
            // Alternating navigation hints exercises optional Selection's
            // engaged-to-empty move assignment on the MSVC STL.
            for ( std::size_t index = 0;
                index <= sed::editor::DocumentMaximumHistoryEntries; ++index )
            {
                if ( index % 2 == 0 )
                {
                    history.apply("Structural history row",
                        static_cast<std::uint32_t>(DocumentDirty::Structure),
                        [](auto&) { return true; });
                }
                else
                {
                    history.pushUndo("Ordinary history row");
                }
            }
            return history;
        };
        auto uncapped = cappedFixture();
        check(uncapped.undo(), "history cap allocation fixture establishes uncapped control");
        std::size_t preparationAllocations{};
        allocationCounter = &preparationAllocations;
        uncapped.pushUndo("New cap boundary row");
        allocationCounter = nullptr;

        auto history = cappedFixture();
        auto expected = cappedFixture();
        bool failed = false;
        allocationsBeforeFailure = preparationAllocations;
        try { history.pushUndo("New cap boundary row"); }
        catch ( const std::bad_alloc& ) { failed = true; }
        allocationsBeforeFailure.reset();
        if ( failed )
        {
            check(sameHistory(history, expected),
                "History cap allocation failure preserves every retained row and next record identity");
            history.pushUndo("After rejected cap trim");
            expected.pushUndo("After rejected cap trim");
            check(history.undoSignature() == expected.undoSignature(),
                "History cap allocation failure preserves the next published record ID");
        }
        else
        {
            expected.pushUndo("New cap boundary row");
            check(sameHistory(history, expected)
                    && history.undoDepth() == sed::editor::DocumentMaximumHistoryEntries,
                "History cap trimming completes without fallible allocation after publication");
        }
    }
    for ( int variant = 0; variant < 10; ++variant )
    {
        bool reachedSuccess = false;
        std::size_t rejected = 0;
        for ( std::size_t position = 0; position < 256; ++position )
        {
            auto history = fixture(0, false);
            auto expected = fixture(0, false);
            bool failed = false;
            allocationsBeforeFailure = position;
            try { pushRecord(history, variant); }
            catch ( const std::bad_alloc& ) { failed = true; }
            allocationsBeforeFailure.reset();
            if ( failed )
            {
                ++rejected;
                check(sameHistory(history, expected),
                    "History push allocation failure preserves state and both stacks");
                history.pushUndo("Next published record");
                expected.pushUndo("Next published record");
                check(history.undoSignature() == expected.undoSignature(),
                    "History push allocation failure preserves the next published record ID");
            }
            else
            {
                pushRecord(expected, variant);
                check(sameHistory(history, expected),
                    "History push succeeds exactly after every fallible allocation was exercised");
                reachedSuccess = true;
                break;
            }
        }
        check(reachedSuccess && rejected != 0,
            "History push allocation sweep exercises rejection and eventual success");
        std::cout << "push variant=" << variant << " rejected=" << rejected << '\n';
    }
    for ( int variant = 0; variant < 9; ++variant )
    {
        for ( const bool redo : {false, true} )
        {
            for ( const bool shared : {false, true} )
            {
                bool reachedSuccess = false;
                std::size_t rejected = 0;
                for ( std::size_t position = 0; position < 256; ++position )
                {
                    auto history = fixture(variant, redo);
                    auto expected = fixture(variant, redo);
                    const auto recovery = shared ? history.compactRecoveryHistory()
                        : sed::editor::DocumentRecoveryHistory{};
                    const auto baselineRecovery = shared ? expected.compactRecoveryHistory()
                        : sed::editor::DocumentRecoveryHistory{};
                    std::uint32_t dirty = 0xdeadbeefu;
                    std::vector<std::int32_t> indices{-77};
                    bool failed = false;
                    bool transitioned = false;
                    allocationsBeforeFailure = position;
                    try
                    {
                        transitioned = redo ? history.redo(&dirty, &indices)
                                            : history.undo(&dirty, &indices);
                    }
                    catch ( const std::bad_alloc& )
                    {
                        failed = true;
                    }
                    allocationsBeforeFailure.reset();
                    if ( failed )
                    {
                        ++rejected;
                        const bool unchanged = sameHistory(history, expected);
                        check(unchanged, "Undo/Redo allocation failure preserves exact state, selection, both stacks and signatures");
                        check(dirty == 0xdeadbeefu && indices == std::vector<std::int32_t>{-77},
                            "Undo/Redo allocation failure preserves caller output parameters");
                        // The next published ID must not skip a value on failure.
                        history.pushUndo("After rejected allocation");
                        expected.pushUndo("After rejected allocation");
                        check(history.undoSignature() == expected.undoSignature(),
                            "Undo/Redo allocation failure preserves subsequent record IDs");
                        if ( !unchanged ) break;
                    }
                    else
                    {
                        std::uint32_t expectedDirty{};
                        std::vector<std::int32_t> expectedIndices{};
                        const bool expectedTransition = redo
                            ? expected.redo(&expectedDirty, &expectedIndices)
                            : expected.undo(&expectedDirty, &expectedIndices);
                        check(transitioned && expectedTransition
                                && dirty == expectedDirty && indices == expectedIndices
                                && sameHistory(history, expected),
                            "Undo/Redo succeeds exactly after every fallible allocation was exercised");
                        reachedSuccess = true;
                    }
                    if ( shared )
                    {
                        check(sameHistory(*recovery.compact, *baselineRecovery.compact),
                            "Undo/Redo never mutates a retained recovery snapshot");
                    }
                    if ( reachedSuccess ) break;
                }
                check(reachedSuccess && (!shared || rejected != 0),
                    "Undo/Redo allocation sweep reaches success and exercises shared snapshot failures");
                std::cout << "variant=" << variant << " redo=" << redo
                          << " shared=" << shared << " rejected=" << rejected << '\n';
            }
        }
    }
    return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
