//
// Created by Steffen Schümann on 15.09.24.
//

#include <emulation/chip8strict.hpp>
#include <emulation/coreregistry.hpp>

namespace emu {

static const std::string PROP_CLASS = "CHIP-8 STRICT";
static const std::string PROP_TRACE_LOG = "Trace Log";
static const std::string PROP_CLOCK = "Clock Rate";
static const std::string PROP_RAM = "Memory";
static const std::string PROP_CLEAN_RAM = "Clean RAM";

Properties Chip8StrictOptions::asProperties() const
{
    auto result = registeredPrototype();
    result[PROP_TRACE_LOG].setBool(traceLog);
    result[PROP_CLOCK].setInt(clockFrequency);
    result[PROP_RAM].setSelectedText(std::to_string(ramSize)); // !!!!
    result[PROP_CLEAN_RAM].setBool(cleanRam);
    return result;
}

Chip8StrictOptions Chip8StrictOptions::fromProperties(const Properties& props)
{
    Chip8StrictOptions opts{};
    opts.traceLog = props[PROP_TRACE_LOG].getBool();
    opts.clockFrequency = props[PROP_CLOCK].getInt();
    opts.ramSize = std::stoul(props[PROP_RAM].getSelectedText()); // !!!!
    opts.cleanRam = props[PROP_CLEAN_RAM].getBool();
    return opts;
}

Properties& Chip8StrictOptions::registeredPrototype()
{
    using namespace std::string_literals;
    auto& prototype = Properties::getProperties(PROP_CLASS);
    if(!prototype) {
        prototype.registerProperty({PROP_TRACE_LOG, false, "Enable trace log", eWritable});
        prototype.registerProperty({PROP_CLOCK, Property::Integer{1760640, 100000, 500'000'000}, "Clock frequency, default is 1760640", eWritable});
        prototype.registerProperty({PROP_RAM, Property::Combo{"2048"s, "4096"s, "8192"s, "12288"s, "16384"s, "32768"s}, "Size of ram in bytes", eWritable});
        prototype.registerProperty({PROP_CLEAN_RAM, false, "Delete ram on startup", eWritable});
    }
    return prototype;
}

struct Chip8StrictSetupInfo {
    const char* presetName;
    const char* description;
    const char* defaultExtensions;
    Chip8StrictOptions options;
};

// clang-format off
Chip8StrictSetupInfo strictPresets[] = {
    {
        "chip-8",
        "The classic CHIP-8 that came from Joseph Weisbecker, 1977",
        ".ch8;.c8vip",
        { .clockFrequency = 1760640, .ramSize = 4096, .cleanRam = true, .traceLog = false}
    }
};
// clang-format on

struct StrictFactoryInfo final : public CoreRegistry::FactoryInfo<Chip8StrictEmulator, Chip8StrictSetupInfo, Chip8StrictOptions>
{
    explicit StrictFactoryInfo(const char* description)
        : FactoryInfo(20, strictPresets, description)
    {}
    std::string prefix() const override
    {
        return "STRICT";
    }
    VariantIndex variantIndex(const Properties& props) const override
    {
        return {0, strictPresets[0].options.asProperties() == props};
    }
};

static bool registeredStrictC8 = CoreRegistry::registerFactory(PROP_CLASS, std::make_unique<StrictFactoryInfo>("First cycle exact HLE emulation of CHIP-8 on a COSMAC VIP"));

}
