#include "test_common.hpp"

#include "sed/assets/jkl.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>

namespace sed::tests
{

void runNdySaveTests()
{
    {
        sed::Level level{};
        level.clear();
        level.kind                   = sed::ProjectType::IJIM;
        level.header.version         = 3;
        level.header.gravity         = 9.0;
        level.ppunit                 = 320.0;
        level.materials              = { "test.mat" };
        level.masterCMP              = "test.cmp";
        level.header.ceilingSky.height = 123.0;

        sed::Sector& sector0 = level.newSector();
        sector0.flags = sed::SECF_CollideBox;

        check(!sed::pointLightHasAuthoredParameters(sector0.pointLight),
            "NDY save: default PointLight is inactive even though ColorBlack alpha is one");
        sed::PointLight activeAtOriginPredicate{};
        activeAtOriginPredicate.color.r = 0.25f;
        check(sed::pointLightHasAuthoredParameters(activeAtOriginPredicate),
            "NDY save: nonzero RGB keeps a sector light active at the world origin");
        activeAtOriginPredicate = sed::PointLight{};
        activeAtOriginPredicate.maxRange = 2.0;
        check(sed::pointLightHasAuthoredParameters(activeAtOriginPredicate),
            "NDY save: nonzero falloff alone is an authored sector-light parameter");
        activeAtOriginPredicate = sed::PointLight{};
        activeAtOriginPredicate.position.x = 1.0;
        check(sed::pointLightHasAuthoredParameters(activeAtOriginPredicate),
            "NDY save: nonzero position alone is preserved as an authored sector-light parameter");

        sector0.ambient    = sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.0f };
        sector0.extraLight = sed::ColorF{ 0.01f, 0.02f, 0.03f, 0.0f };
        sector0.tint.setToZero();

        sector0.addVertex(0.0, 0.0, 0.0);
        sector0.addVertex(1.0, 0.0, 0.0);
        sector0.addVertex(0.0, 1.0, 0.0);

        sed::Surface surface{};
        surface.material = "test.mat";
        surface.vertices = { 0, 1, 2 };
        surface.texVertices = {
            sed::TexVertex{ 0.0f, 0.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
            sed::TexVertex{ 1.0f, 0.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
            sed::TexVertex{ 0.0f, 1.0f, sed::ColorF{ 1.0f, 1.0f, 1.0f, 1.0f } },
        };
        sector0.addSurface(std::move(surface));

        std::ostringstream oss;
        sed::assets::saveToJKL(level, oss);

        const std::string saved = oss.str();
        check(saved.find("BOUNDBOX ") != std::string::npos, "NDY save: writes BOUNDBOX");
        check(saved.find("CENTER ") != std::string::npos, "NDY save: writes CENTER");
        check(saved.find("RADIUS ") != std::string::npos, "NDY save: writes RADIUS");
        check(saved.find("COLLIDEBOX ") == std::string::npos, "NDY save: omits COLLIDEBOX when zero");
        check(saved.find("AVERAGE LIGHT ") == std::string::npos,
            "NDY save: default black PointLight alpha does not serialize an absent average light");

        std::istringstream iss(saved);
        sed::assets::JklLoadOptions opts{};
        opts.projectType = sed::ProjectType::IJIM;
        const sed::Level loaded = sed::assets::loadFromJKL(iss, opts);

        check(loaded.kind == sed::ProjectType::IJIM, "NDY save: kind");
        check(loaded.sectors.size() == 1, "NDY save: sectors");

        if (!loaded.sectors.empty())
        {
            const sed::Sector& s0 = loaded.sectors[0];
            check((s0.flags & sed::SECF_CollideBox) == 0, "NDY save: clears SECF_CollideBox on write");
            check(close(static_cast<double>(s0.ambient.r), 0.1), "NDY save: ambient.r");
            check(close(static_cast<double>(s0.ambient.g), 0.2), "NDY save: ambient.g");
            check(close(static_cast<double>(s0.ambient.b), 0.3), "NDY save: ambient.b");
        }

        sector0.pointLight.color = sed::ColorF{ 0.25f, 0.5f, 1.0f, 1.0f };
        sector0.pointLight.position = sed::TVector::zero;
        sector0.pointLight.minRange = 0.0;
        sector0.pointLight.maxRange = 4.0;
        std::ostringstream activeAtOriginStream;
        sed::assets::saveToJKL(level, activeAtOriginStream);
        const std::string activeAtOrigin = activeAtOriginStream.str();
        check(activeAtOrigin.find("AVERAGE LIGHT INTENSITY ") != std::string::npos,
            "NDY save: active sector light at world origin writes its nonzero RGB");
        check(activeAtOrigin.find("AVERAGE LIGHT FALLOFF ") != std::string::npos,
            "NDY save: active sector light at world origin writes its nonzero falloff pair");
        check(activeAtOrigin.find("AVERAGE LIGHT POSITION ") == std::string::npos,
            "NDY save: active sector light at world origin omits the zero position record");
        std::istringstream activeAtOriginInput(activeAtOrigin);
        const sed::Level activeAtOriginReloaded =
            sed::assets::loadFromJKL(activeAtOriginInput, opts);
        check(activeAtOriginReloaded.sectors.size() == 1
                && close(activeAtOriginReloaded.sectors[0].pointLight.color.r, 0.25)
                && close(activeAtOriginReloaded.sectors[0].pointLight.color.g, 0.5)
                && close(activeAtOriginReloaded.sectors[0].pointLight.color.b, 1.0)
                && close(activeAtOriginReloaded.sectors[0].pointLight.minRange, 0.0)
                && close(activeAtOriginReloaded.sectors[0].pointLight.maxRange, 4.0),
            "NDY save: active sector-light RGB and falloff reload semantically");

        sector0.pointLight = sed::PointLight{};
        sector0.pointLight.position = sed::TVector{ 1.0, 2.0, 3.0 };
        std::ostringstream positionOnlyStream;
        sed::assets::saveToJKL(level, positionOnlyStream);
        const std::string positionOnly = positionOnlyStream.str();
        check(positionOnly.find("AVERAGE LIGHT POSITION ") != std::string::npos
                && positionOnly.find("AVERAGE LIGHT INTENSITY ") == std::string::npos
                && positionOnly.find("AVERAGE LIGHT FALLOFF ") == std::string::npos,
            "NDY save: independent position-only average-light parameters round-trip without synthetic fields");
        std::istringstream positionOnlyInput(positionOnly);
        const sed::Level positionOnlyReloaded =
            sed::assets::loadFromJKL(positionOnlyInput, opts);
        check(positionOnlyReloaded.sectors.size() == 1
                && close(positionOnlyReloaded.sectors[0].pointLight.position.x, 1.0)
                && close(positionOnlyReloaded.sectors[0].pointLight.position.y, 2.0)
                && close(positionOnlyReloaded.sectors[0].pointLight.position.z, 3.0),
            "NDY save: independent position-only average-light payload reloads semantically");
    }

    {
        const std::filesystem::path fixture =
            "src/core/tests/assets/fixtures/ijim_thing_params.ndy";
        sed::assets::JklLoadOptions loadOptions{};
        loadOptions.projectType = sed::ProjectType::IJIM;
        sed::Level loaded = sed::assets::loadFromJKL(fixture, loadOptions);

        check(loaded.kind == sed::ProjectType::IJIM,
            "NDY Thing params integration: project kind");
        check(loaded.templates.size() == 2,
            "NDY Thing params integration: template count");
        check(loaded.things.size() == 1,
            "NDY Thing params integration: Thing count");

        const auto findValue = [](const auto& values, const std::string_view name) -> const sed::TplValue*
        {
            for (const sed::TplValue& value : values)
            {
                if (sed::text::iequalsAscii(value.name, name))
                {
                    return &value;
                }
            }
            return nullptr;
        };

        if (loaded.templates.size() >= 1)
        {
            const sed::ThingTemplate& base = loaded.templates[0];
            const sed::TplValue* light = findValue(base.values, "light");
            const sed::TplValue* intensity = findValue(base.values, "lightintensity");
            const sed::TplValue* offset = findValue(base.values, "lightoffset");
            const sed::TplValue* damageClass = findValue(base.values, "damageclass");
            check(light && light->asString() == "(0.1/0.2/0.3/4)"
                    && intensity && intensity->asString() == "(0.4/0.5/0.6/1)"
                    && offset && offset->asString() == "(0/.07/.04)",
                "NDY Thing params integration: template RGB Headlight materializes editor range");
            check(damageClass && damageClass->type == sed::ValueType::flag
                    && damageClass->intValue == 0x20,
                "NDY Thing params integration: template flags deserialize by type");
        }

        if (!loaded.things.empty())
        {
            sed::Thing& thing = loaded.things[0];
            const sed::TplValue* light = findValue(thing.values, "light");
            const sed::TplValue* intensity = findValue(thing.values, "lightintensity");
            const sed::TplValue* offset = findValue(thing.values, "lightoffset");
            check(light && light->asString() == "(0.25/0.5/0.75/8)"
                    && intensity && intensity->asString() == "(1/0.5/0.25/1)"
                    && offset && offset->asString() == "(1/2/3)",
                "NDY Thing params integration: local RGB Headlight materializes editor range");

            std::size_t frameCount = 0;
            for (const sed::TplValue& value : thing.values)
            {
                if (sed::text::iequalsAscii(value.name, "frame"))
                {
                    ++frameCount;
                }
            }
            check(frameCount == 2,
                "NDY Thing params integration: repeated frame parameters deserialize in order");

            // Emulate a stale pre-fix editor value to verify the writer itself
            // enforces canonical syntax, independently of UI formatting.
            for (sed::TplValue& value : thing.values)
            {
                if (sed::text::iequalsAscii(value.name, "light"))
                {
                    value.stringValue = "0.25/0.5/0.75/8";
                }
                else if (sed::text::iequalsAscii(value.name, "lightintensity"))
                {
                    value.stringValue = "1/0.5/0.25/7";
                }
                else if (sed::text::iequalsAscii(value.name, "lightoffset"))
                {
                    value.stringValue = "1/2/3";
                }
            }
        }

        if (!loaded.templates.empty())
        {
            // Exercise the template writer independently of the Thing writer.
            // These emulate values retained by an older SED document model.
            for (sed::TplValue& value : loaded.templates[0].values)
            {
                if (sed::text::iequalsAscii(value.name, "light"))
                {
                    value.stringValue = "0.1/0.2/0.3/4";
                }
                else if (sed::text::iequalsAscii(value.name, "lightintensity"))
                {
                    value.stringValue = "0.4/0.5/0.6/2.5";
                }
                else if (sed::text::iequalsAscii(value.name, "lightoffset"))
                {
                    value.stringValue = "0/.07/.04";
                }
            }
        }

        const auto unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path savedPath =
            "tmp_ndy_thing_params_" + unique + ".ndy";
        try
        {
            sed::assets::saveToJKL(loaded, savedPath);

            std::ifstream savedFile(savedPath, std::ios::binary);
            const std::string savedText{
                std::istreambuf_iterator<char>(savedFile),
                std::istreambuf_iterator<char>() };
            check(savedText.find("light=(0.25/0.5/0.75/8)") != std::string::npos
                    && savedText.find("lightintensity=(1/0.5/0.25)") != std::string::npos
                    && savedText.find("lightintensity=(1/0.5/0.25/7)") == std::string::npos
                    && savedText.find("lightoffset=(1/2/3)") != std::string::npos,
                "NDY Thing params integration: writer repairs tuples and strips unsupported Headlight range");
            check(savedText.find("light=(0.1/0.2/0.3/4)") != std::string::npos
                    && savedText.find("lightintensity=(0.4/0.5/0.6)") != std::string::npos
                    && savedText.find("lightintensity=(0.4/0.5/0.6/2.5)") == std::string::npos
                    && savedText.find("lightoffset=(0/.07/.04)") != std::string::npos
                    && savedText.find("damageclass=0x20") != std::string::npos,
                "NDY Thing params integration: template writer emits retail-compatible Headlight RGB");
            check(savedText.find("frame=(4/5/6:10/20/30) frame=(7/8/9)")
                        != std::string::npos
                    && savedText.find("material=folder/1/2/test.mat") != std::string::npos
                    && savedText.find("vel=(0/4/0)") != std::string::npos
                    && savedText.find("voicecolor=(1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16)")
                        != std::string::npos,
                "NDY Thing params integration: repeated frames, paths, and vectors serialize safely");

            const sed::Level reparsed = sed::assets::loadFromJKL(savedPath, loadOptions);
            check(reparsed.templates.size() == 2 && reparsed.things.size() == 1,
                "NDY Thing params integration: saved file reloads");
            if (!reparsed.things.empty())
            {
                const sed::TplValue* reparsedLight =
                    findValue(reparsed.things[0].values, "light");
                const sed::TplValue* reparsedIntensity =
                    findValue(reparsed.things[0].values, "lightintensity");
                const sed::TplValue* reparsedOffset =
                    findValue(reparsed.things[0].values, "lightoffset");
                check(reparsedLight && reparsedLight->asString() == "(0.25/0.5/0.75/8)"
                        && reparsedIntensity && reparsedIntensity->asString() == "(1/0.5/0.25/1)"
                        && reparsedOffset && reparsedOffset->asString() == "(1/2/3)",
                    "NDY Thing params integration: save-load resets unsupported Headlight range to editor default");
            }
        }
        catch (const std::exception& error)
        {
            check(false, error.what());
        }

        std::error_code ec;
        std::filesystem::remove(savedPath, ec);
    }
}

} // namespace sed::tests
