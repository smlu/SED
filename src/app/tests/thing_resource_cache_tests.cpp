#include "test_common.hpp"
#include "resource_release_test_fixtures.hpp"
#include "thing/thing_resource_utils.hpp"
#include <QByteArray>
#include <QTemporaryDir>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace sed::app_tests
{
namespace
{
void writeResource(const std::filesystem::path& path, const QByteArray& bytes, int revision)
{
    std::ofstream(path, std::ios::binary).write(bytes.data(), bytes.size());
    std::filesystem::last_write_time(path,
        std::filesystem::file_time_type::clock::now() + std::chrono::seconds(revision));
}
}
void runThingResourceCacheTests()
{
    for (const bool sprite : {false, true})
    {
        assets::ResourceLocator unreadable;
        const std::string name = sprite ? "failed_bounds.spr" : "failed_bounds.3do";
        int reads = 0;
        unreadable.setPrimaryResourceOverride(name, [&]() -> std::optional<std::vector<std::byte>>
            {
                ++reads;
                throw std::runtime_error("Truncated Thing support asset");
            });
        bool threw = false;
        bool available = false;
        try
        {
            for (int attempt = 0; attempt < 2; ++attempt)
                available = bool(sprite ? thing::loadSpriteBox(unreadable, name)
                                        : thing::loadModel3doBox(unreadable, name)) || available;
        }
        catch (const std::exception&) { threw = true; }
        check(!threw && !available && reads == 1,
            "Unreadable model or sprite support asset keeps unavailable fallback without UI exception");
    }
    QTemporaryDir firstDirectory, secondDirectory;
    check(firstDirectory.isValid() && secondDirectory.isValid(), "Thing bounds cache temporary fixtures exist");
    if (!firstDirectory.isValid() || !secondDirectory.isValid()) return;
    const auto firstPath = std::filesystem::path(firstDirectory.path().toStdWString());
    const auto secondPath = std::filesystem::path(secondDirectory.path().toStdWString());
    assets::ResourceLocator first, second;
    first.addRoot(firstPath);
    second.addRoot(secondPath);
    writeResource(firstPath / "release_bounds.3do", fixtures::makeDiskChangeModelFixture("unused.mat", "first", 1), 0);
    writeResource(secondPath / "release_bounds.3do", fixtures::makeDiskChangeModelFixture("unused.mat", "second", 4), 0);
    writeResource(firstPath / "release_bounds.spr", "unused.mat 0 2 2 4 3 3 0 0 0 0\n", 0);
    writeResource(secondPath / "release_bounds.spr", "unused.mat 0 8 2 4 3 3 0 0 0 0\n", 0);
    const auto modelFirst = thing::loadModel3doBox(first, "release_bounds");
    const auto modelSecond = thing::loadModel3doBox(second, "release_bounds");
    const auto spriteFirst = thing::loadSpriteBox(first, "release_bounds");
    const auto spriteSecond = thing::loadSpriteBox(second, "release_bounds");
    check(modelFirst && modelFirst->x2 == 1 && modelSecond && modelSecond->x2 == 4,
        "Thing support model bounds do not reuse another project's same-name resource");
    check(spriteFirst && spriteFirst->x2 == 1 && spriteSecond && spriteSecond->x2 == 4,
        "Thing support sprite bounds do not reuse another project's same-name resource");
    writeResource(firstPath / "release_bounds.3do", fixtures::makeDiskChangeModelFixture("unused.mat", "new disk revision", 3), 10);
    writeResource(firstPath / "release_bounds.spr", "unused.mat 0 6 2 4 3 3 0 0 0 0\n", 10);
    const auto changedModel = thing::loadModel3doBox(first, "release_bounds");
    const auto changedSprite = thing::loadSpriteBox(first, "release_bounds");
    check(changedModel && changedModel->x2 == 3 && changedSprite && changedSprite->x2 == 3,
        "Thing support bounds reflect changed disk resources without restarting editor");
    check(!thing::loadModel3doBox(first, "release_created_later")
            && !thing::loadSpriteBox(first, "release_created_later"),
        "Thing support missing-resource fallback fixture starts unavailable");
    writeResource(firstPath / "release_created_later.3do", fixtures::makeDiskChangeModelFixture("unused.mat", "created later", 2), 20);
    writeResource(firstPath / "release_created_later.spr", "unused.mat 0 4 2 4 3 3 0 0 0 0\n", 20);
    const auto createdModel = thing::loadModel3doBox(first, "release_created_later");
    const auto createdSprite = thing::loadSpriteBox(first, "release_created_later");
    check(createdModel && createdModel->x2 == 2 && createdSprite && createdSprite->x2 == 2,
        "Thing support missing-resource cache invalidates when model or sprite is created");
}
}
