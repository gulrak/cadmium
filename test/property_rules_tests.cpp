#include <doctest/doctest.h>
#include <emuhostex.hpp>

#include <string>

namespace {

emu::Properties rulesApplied(std::string_view preset)
{
    auto props = emu::CoreRegistry::propertiesForPreset(preset);
    REQUIRE(props);
    emu::CoreRegistry::applyRules(props);
    return props;
}

emu::PropertyState stateOf(const emu::Properties& props, std::string_view id)
{
    return props.at(id).getState();
}

} // namespace

TEST_CASE("The rules leave the values of every preset unchanged")
{
    emu::CoreRegistry registry;
    for (const auto& [name, info] : registry) {
        for (size_t i = 0; i < info->numberOfVariants(); ++i) {
            const auto preset = info->variantProperties(i);
            auto props = preset;
            emu::CoreRegistry::applyRules(props);
            const std::string context = std::string(name) + " / " + info->variantName(i);
            INFO(context);
            CHECK(props == preset);
        }
    }
}

TEST_CASE("The HP-48 display is only applicable to the HP-48SX variants")
{
    for (const auto* preset : {"CHIP-48", "SCHIP-1.0-BETA", "SCHIP-1.0", "SCHIP-1.1", "SCHIPC"}) {
        INFO(preset);
        CHECK(stateOf(rulesApplied(preset), "hp48Display") == emu::PropertyState::ACTIVE);
    }
    for (const auto* preset : {"CHIP-8", "CHIP-8X", "SCHIP-MODERN", "MEGACHIP", "XO-CHIP"}) {
        INFO(preset);
        CHECK(stateOf(rulesApplied(preset), "hp48Display") == emu::PropertyState::NOT_APPLICABLE);
    }
}

TEST_CASE("A stale value of a not applicable property goes back to the preset default")
{
    auto props = emu::CoreRegistry::propertiesForPreset("CHIP-8");
    props.at("hp48Display").setBool(true);
    emu::CoreRegistry::applyRules(props);
    CHECK_FALSE(props.at("hp48Display").getBool());
    CHECK(props == emu::CoreRegistry::propertiesForPreset("CHIP-8"));
}

TEST_CASE("The SCHIP drawing quirks only apply to SCHIP drawing and need wrapping off")
{
    auto props = rulesApplied("SCHIP-1.1");
    CHECK(stateOf(props, "schipLoresDrawing") == emu::PropertyState::ACTIVE);
    CHECK(stateOf(props, "schip11Collision") == emu::PropertyState::ACTIVE);

    props.at("wrapSprites").setBool(true);
    emu::CoreRegistry::applyRules(props, "wrapSprites");
    CHECK(stateOf(props, "schipLoresDrawing") == emu::PropertyState::INACTIVE);
    CHECK(stateOf(props, "schip11Collision") == emu::PropertyState::INACTIVE);
    CHECK(props.at("schipLoresDrawing").getBool());  // inactive keeps the value

    for (const auto* preset : {"CHIP-8", "MEGACHIP", "XO-CHIP"}) {
        INFO(preset);
        CHECK(stateOf(rulesApplied(preset), "schipLoresDrawing") == emu::PropertyState::NOT_APPLICABLE);
    }
}

TEST_CASE("The extended vblank wait is inactive with instant Dxyn")
{
    auto props = rulesApplied("CHIP-8");
    CHECK(stateOf(props, "extendedVblank") == emu::PropertyState::ACTIVE);
    props.at("instantDxyn").setBool(true);
    emu::CoreRegistry::applyRules(props, "instantDxyn");
    CHECK(stateOf(props, "extendedVblank") == emu::PropertyState::INACTIVE);
    CHECK(props.at("extendedVblank").getBool());
}

TEST_CASE("Switching on one of the load/store quirks switches off the other")
{
    auto props = rulesApplied("CHIP-48");
    REQUIRE(props.at("loadStoreIncIByx").getBool());
    props.at("loadStoreIncIByXPlus1").setBool(true);
    emu::CoreRegistry::applyRules(props, "loadStoreIncIByXPlus1");
    CHECK(props.at("loadStoreIncIByXPlus1").getBool());
    CHECK_FALSE(props.at("loadStoreIncIByx").getBool());

    props.at("loadStoreIncIByx").setBool(true);
    emu::CoreRegistry::applyRules(props, "loadStoreIncIByx");
    CHECK(props.at("loadStoreIncIByx").getBool());
    CHECK_FALSE(props.at("loadStoreIncIByXPlus1").getBool());
}

TEST_CASE("A loaded configuration with both load/store quirks keeps the one the core uses")
{
    auto props = emu::CoreRegistry::propertiesForPreset("CHIP-8");
    props.at("loadStoreIncIByXPlus1").setBool(true);
    props.at("loadStoreIncIByx").setBool(true);
    emu::CoreRegistry::applyRules(props);
    CHECK(props.at("loadStoreIncIByx").getBool());
    CHECK_FALSE(props.at("loadStoreIncIByXPlus1").getBool());
}

TEST_CASE("The host applies the rules to a requested configuration")
{
    emu::HeadlessHost host;
    auto props = emu::CoreRegistry::propertiesForPreset("CHIP-8");
    props.at("hp48Display").setBool(true);
    host.updateEmulatorOptions(props);
    const auto& active = host.getProperties();
    CHECK_FALSE(active.at("hp48Display").getBool());
    CHECK(active.at("hp48Display").getState() == emu::PropertyState::NOT_APPLICABLE);
}
