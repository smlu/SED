#include "test_common.hpp"
#include "sed/assets/resource_locator.hpp"
#include <stdexcept>
#include <atomic>
#include <thread>
#include <string>

namespace sed::tests
{
void runResourceLocatorOverlayTests()
{
    assets::ResourceLocator source;
    source.addRoot("resource-overlay-base");
    const auto* config = &source.roots();
    int reads = 0;
    source.setPrimaryResourceOverride("3DO\\same.3do", [&]()
        -> std::optional<std::vector<std::byte>>
    {
        ++reads;
        return std::vector<std::byte>{std::byte{'a'}, std::byte{'b'}};
    });
    const auto overlayIdentity = source.previewCacheIdentity();
    const auto revision = source.primaryResourceOverrideRevision("same.3do");
    check(revision && reads == 0 && &source.roots() == config,
        "ResourceLocator lazy overlay revision is I/O-free and shares source configuration");
    assets::ResourceLocator copy = source;
    check(copy.primaryResourceOverrideRevision("  HI3DO/same.3do  ") == revision,
        "ResourceLocator primary override recognizes canonical folder/case/slash aliases");
    check(source.tryReadText("same.3do") == std::optional<std::string>("ab")
            && copy.tryReadText("3do/SAME.3DO") == std::optional<std::string>("ab")
            && reads == 1,
        "ResourceLocator copied overlay materializes source bytes only once");
    assets::ResourceLocator concurrent;
    std::atomic<int> concurrentReads{0}, matchingReads{0};
    concurrent.setPrimaryResourceOverride("shared.spr", [&]()
    {
        ++concurrentReads;
        return std::optional<std::vector<std::byte>>(std::vector<std::byte>{std::byte{'x'}});
    });
    {
        std::vector<std::jthread> workers;
        for (int i = 0; i < 8; ++i)
            workers.emplace_back([snapshot = concurrent, &matchingReads]()
            { if (snapshot.tryReadText("shared.spr") == std::optional<std::string>("x")) ++matchingReads; });
    }
    check(concurrentReads == 1 && matchingReads == 8,
        "ResourceLocator concurrent copied readers materialize one synchronized immutable payload");
    copy.clearPrimaryResourceOverride();
    check(!copy.primaryResourceOverrideRevision("same.3do")
            && copy.previewCacheIdentity() != overlayIdentity
            && source.primaryResourceOverrideRevision("same.3do") == revision
            && &copy.roots() == config,
        "ResourceLocator removing primary override preserves retained copies and shared configuration");
    const auto unchanged = source.previewCacheIdentity();
    bool rejected = false;
    try { source.setPrimaryResourceOverride("", [] { return std::optional<std::vector<std::byte>>{}; }); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && source.previewCacheIdentity() == unchanged
            && source.tryReadText("same.3do") == std::optional<std::string>("ab"),
        "ResourceLocator invalid overlay replacement preserves previous owner and identity");
    int failedReads = 0;
    source.setPrimaryResourceOverride("same.3do", [&]() -> std::optional<std::vector<std::byte>>
    { ++failedReads; throw std::runtime_error("deterministic selected-source failure"); });
    auto failedCopy = source;
    int caught = 0;
    try { (void)source.tryReadBinary("same.3do"); } catch (const std::runtime_error&) { ++caught; }
    try { (void)failedCopy.tryReadBinary("same.3do"); } catch (const std::runtime_error&) { ++caught; }
    check(caught == 2 && failedReads == 1,
        "ResourceLocator copied lazy overlay caches read failure until explicit replacement");
    source.setPrimaryResourceOverride("same.3do", []
        { return std::optional<std::vector<std::byte>>(std::vector<std::byte>{}); });
    const auto empty = source.tryReadBinaryWithExt("same", ".3do");
    check(empty && empty->empty(), "ResourceLocator zero-byte primary override is present with extension fallback");
    source.setPrimaryResourceOverride("same.3do", []
        { return std::optional<std::vector<std::byte>>{}; });
    check(!source.tryReadBinary("same.3do"), "ResourceLocator missing selected override remains missing");
}
}
