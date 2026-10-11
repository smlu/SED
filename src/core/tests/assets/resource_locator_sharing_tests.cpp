#include "test_common.hpp"
#include "resource_locator_test_fixtures.hpp"
#include "sed/assets/resource_locator.hpp"
#include <utility>

namespace sed::tests
{
void runResourceLocatorSharingTests()
{
    ResourceLocatorFixture fixture;
    assets::ResourceLocator original;
    original.addRoot("first-resource-root");
    original.mountGob(fixture.firstGob);
    assets::ResourceLocator copy = original;
    check(&copy.roots() == &original.roots() && &copy.gobs() == &original.gobs(),
        "ResourceLocator copies share immutable root and mounted-index configuration");
    check(!copy.gobs().empty() && copy.gobs()[0].entries().size() == 1
            && copy.gobs()[0].entries().data() == original.gobs()[0].entries().data(),
        "ResourceLocator copies share nonempty mounted archive entry storage");
    const auto* retainedRoots = &copy.roots();
    const auto copiedIdentity = copy.previewCacheIdentity();
    original.addRoot("second-resource-root");
    check(&original.roots() != retainedRoots && &copy.roots() == retainedRoots
            && copy.roots().size() == 1 && original.roots().size() == 2
            && copy.previewCacheIdentity() == copiedIdentity
            && original.previewCacheIdentity() != copiedIdentity,
        "ResourceLocator mutation detaches configuration and retains copied snapshot");
    assets::ResourceLocator moved(std::move(copy));
    check(&moved.roots() == retainedRoots && moved.previewCacheIdentity() == copiedIdentity
            && copy.roots().empty() && copy.gobs().empty() && copy.previewCacheIdentity() == 0,
        "ResourceLocator move transfers shared configuration and empties source coherently");
    const auto* beforeFailedMount = &original.roots();
    const auto identityBeforeFailure = original.previewCacheIdentity();
    bool threw = false;
    try { original.mountGob("missing-resource-sharing-test-archive.gob"); }
    catch (...) { threw = true; }
    check(threw && &original.roots() == beforeFailedMount
            && original.previewCacheIdentity() == identityBeforeFailure,
        "ResourceLocator failed mutation preserves exact configuration owner and identity");
}
}
