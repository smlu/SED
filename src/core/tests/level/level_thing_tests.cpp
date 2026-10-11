#include "test_common.hpp"

#include "sed/level/thing.hpp"

namespace sed::tests
{

void runLevelThingTests()
{
    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;

        sed::ThingTemplate base{};
        base.name = "base";
        base.parent = "none";

        sed::TplValue baseFlags{};
        baseFlags.name = "thingflags";
        baseFlags.type = sed::ValueType::flag;
        check(baseFlags.parse("0x10"), "Thing: base thingflags parse");
        base.values.push_back(baseFlags);

        sed::TplValue baseLight{};
        baseLight.name = "light";
        baseLight.type = sed::ValueType::str;
        check(baseLight.parse("0.25"), "Thing: base light parse");
        base.values.push_back(baseLight);

        sed::TplValue baseModel{};
        baseModel.name = "model3do";
        baseModel.type = sed::ValueType::str;
        check(baseModel.parse("base.3do"), "Thing: base model parse");
        base.values.push_back(baseModel);

        level.templates.push_back(base);

        sed::ThingTemplate child{};
        child.name = "walkplayer";
        child.parent = "base";

        sed::TplValue childModel{};
        childModel.name = "model3d";
        childModel.type = sed::ValueType::str;
        check(childModel.parse("player.3do"), "Thing: child model parse");
        child.values.push_back(childModel);

        level.templates.push_back(child);

        sed::Thing thing{};
        thing.name = "walkplayer";

        const sed::TplValue* model = sed::level::thingFindValue(level, thing, "model3d");
        check(model != nullptr && model->stringValue == "player.3do", "Thing: model from child template");

        const sed::TplValue* inheritedFlags = sed::level::thingFindValue(level, thing, "thingflags");
        check(inheritedFlags != nullptr && inheritedFlags->intValue == 0x10, "Thing: thingflags inherited");

        check(sed::level::thingFlags(level, thing) == 0x10, "Thing: thingFlags()");

        const sed::ColorF light = sed::level::thingLight(level, thing);
        check(close(light.r, 0.25) && close(light.g, 0.25) && close(light.b, 0.25) && close(light.a, 1.0),
            "Thing: thingLight JKDF2");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::IJIM;

        sed::ThingTemplate tpl{};
        tpl.name = "testthing";
        tpl.parent = "none";

        sed::TplValue light{};
        light.name = "light";
        light.type = sed::ValueType::str;
        check(light.parse("0.1/0.2/0.3/0.4"), "Thing: IJIM light parse");
        tpl.values.push_back(light);

        level.templates.push_back(tpl);

        sed::Thing thing{};
        thing.name = "testthing";

        const sed::ColorF c = sed::level::thingLight(level, thing);
        check(close(c.r, 0.1) && close(c.g, 0.2) && close(c.b, 0.3) && close(c.a, 0.4),
            "Thing: thingLight IJIM color");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;

        sed::ThingTemplate tpl{};
        tpl.name = "base";
        tpl.parent = "none";

        sed::TplValue flags{};
        flags.name = "thingflags";
        flags.type = sed::ValueType::flag;
        check(flags.parse("0x10"), "Thing: template thingflags parse");
        tpl.values.push_back(flags);

        level.templates.push_back(tpl);

        sed::Thing thing{};
        thing.name = "base";

        sed::TplValue overrideFlags{};
        overrideFlags.name = "thingflags";
        overrideFlags.type = sed::ValueType::flag;
        check(overrideFlags.parse("0x80000"), "Thing: override thingflags parse");
        thing.values.push_back(overrideFlags);

        check(sed::level::thingFlags(level, thing) == 0x80000, "Thing: thingFlags override");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;

        sed::ThingTemplate base{};
        base.name = "duplicate_scalars";
        base.parent = "none";

        sed::TplValue firstTemplateLight{};
        firstTemplateLight.name = "light";
        firstTemplateLight.type = sed::ValueType::str;
        check(firstTemplateLight.parse("0.25"), "Thing: first duplicate template light parse");
        base.values.push_back(firstTemplateLight);

        sed::TplValue lastTemplateLight{};
        lastTemplateLight.name = "LIGHT";
        lastTemplateLight.type = sed::ValueType::str;
        check(lastTemplateLight.parse("0.75"), "Thing: last duplicate template light parse");
        base.values.push_back(lastTemplateLight);
        level.templates.push_back(base);

        sed::Thing thing{};
        thing.name = "duplicate_scalars";

        const sed::TplValue* inherited = sed::level::thingFindValue(level, thing, "light");
        check(inherited != nullptr && inherited->asString() == "0.75",
            "Thing: duplicate template scalar lookup is last-wins");

        sed::TplValue firstLocalLight{};
        firstLocalLight.name = "light";
        firstLocalLight.type = sed::ValueType::str;
        check(firstLocalLight.parse("0.5"), "Thing: first duplicate local light parse");
        thing.values.push_back(firstLocalLight);

        sed::TplValue lastLocalLight{};
        lastLocalLight.name = "LIGHT";
        lastLocalLight.type = sed::ValueType::str;
        check(lastLocalLight.parse("0.9"), "Thing: last duplicate local light parse");
        thing.values.push_back(lastLocalLight);

        const sed::TplValue* local = sed::level::thingFindValue(level, thing, "light");
        check(local != nullptr && local->asString() == "0.9",
            "Thing: duplicate local scalar lookup is last-wins");
        const sed::ColorF color = sed::level::thingLight(level, thing);
        check(close(color.r, 0.9) && close(color.g, 0.9) && close(color.b, 0.9),
            "Thing: typed scalar consumer uses last local occurrence");
    }
}

} // namespace sed::tests
