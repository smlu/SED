#include "test_common.hpp"
#include "resource_release_test_fixtures.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/level.hpp"
#include <QTemporaryDir>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace sed
{
double calcThingInsertOffsetZForTest(const Level&, const assets::ResourceLocator&, std::string_view);
}
namespace sed::app_tests
{
void runProjectResourcePrecisionTests()
{
    QTemporaryDir first, second;
    check(first.isValid() && second.isValid(), "startup resource offset fixture roots exist");
    if (!first.isValid() || !second.isValid()) return;
    const auto firstPath = std::filesystem::path(first.path().toStdWString());
    const auto secondPath = std::filesystem::path(second.path().toStdWString());
    assets::ResourceLocator a, b;
    a.addRoot(firstPath); b.addRoot(secondPath);
    for (bool sprite : {false, true})
    {
        const std::string name = sprite ? "startup-offset.spr" : "startup-offset.3do";
        const auto write = [&](const std::filesystem::path& root, double offset)
        {
            QByteArray bytes;
            if (sprite)
                bytes = "fallback.mat 0 2.0 1.0 4 3 3 0.0 0.0 0.0 "
                    + QByteArray::number(offset, 'g', 17) + "\n";
            else
            {
                bytes = fixtures::makeDiskChangeModelFixture("fallback.mat", "startup offset", 2.0);
                bytes.replace("INSERT OFFSET 0 0 0", "INSERT OFFSET 0 0 " + QByteArray::number(offset, 'g', 17));
            }
            std::ofstream out(root / name, std::ios::binary);
            out.write(bytes.data(), bytes.size());
            check(out.good(), "startup resource offset fixture writes complete resource");
        };
        Level level;
        ThingTemplate tpl;
        tpl.name = "startup_offset";
        tpl.parent = "none";
        TplValue value;
        value.name = sprite ? "sprite" : "model3d";
        value.type = ValueType::str;
        value.stringValue = name;
        tpl.values.push_back(value);
        level.templates.push_back(tpl);
        assets::ResourceLocator unreadable;
        int readCount = 0;
        unreadable.setPrimaryResourceOverride(name, [&]() -> std::optional<std::vector<std::byte>>
        {
            ++readCount;
            throw std::runtime_error("Unreadable startup resource");
        });
        bool escaped = false;
        double fallback = -1.0;
        try
        {
            fallback = calcThingInsertOffsetZForTest(level, unreadable, tpl.name);
            fallback += calcThingInsertOffsetZForTest(level, unreadable, tpl.name);
        }
        catch (const std::exception&) { escaped = true; }
        check(!escaped && fallback == 0.0 && readCount == 1,
            "startup insertion handles unreadable model or sprite with cached zero offset");
        write(firstPath, 0.125); write(secondPath, 0.375);
        check(calcThingInsertOffsetZForTest(level, a, tpl.name) == 0.125
                && calcThingInsertOffsetZForTest(level, b, tpl.name) == 0.375,
            "startup player insertion uses each project's same-name model or sprite offset");
        const auto before = std::filesystem::last_write_time(firstPath / name);
        write(firstPath, 0.625);
        std::filesystem::last_write_time(firstPath / name, before + std::chrono::seconds(2));
        check(calcThingInsertOffsetZForTest(level, a, tpl.name) == 0.625,
            "startup player insertion refreshes changed resource offset");
    }
}
}
