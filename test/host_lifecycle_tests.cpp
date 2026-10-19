#include <doctest/doctest.h>
#include <emuhostex.hpp>
#include <configuration.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace {

class LifecycleHost : public emu::HeadlessHost
{
public:
    std::optional<uint8_t> keyDown;
    bool isKeyDown(uint8_t key) override { return keyDown == key; }
    int emuChanged{0};
    int machineResets{0};
    int romLoaded{0};
    std::string lastSource;
    const emu::Palette& palette() const { return _colorPalette; }
    const emu::Palette& propertiesPalette() const { return _properties->palette(); }
    bool loadFromEditor(const std::vector<uint8_t>& code) { return replaceProgramFromEditor(code, calculateSha1(code)); }

protected:
    void whenEmuChanged(emu::IEmulationCore&) override { ++emuChanged; }
    void whenMachineReset() override { ++machineResets; }
    void whenRomLoaded(const std::string&, bool, emu::OctoCompiler*, const std::string& source) override
    {
        ++romLoaded;
        lastSource = source;
    }
};

struct Delta
{
    int coreCreations;
    int resets;
    int programLoads;
};

// Runs an operation and returns how often each lifecycle step happened during it.
template<typename Operation>
Delta countDuring(const LifecycleHost& host, Operation&& operation)
{
    const auto before = host.lifecycleCounters();
    operation();
    const auto& after = host.lifecycleCounters();
    return {after.coreCreations - before.coreCreations, after.resets - before.resets,
            after.programLoads - before.programLoads};
}

const std::vector<uint8_t> CODE{0x60, 0x2A, 0x12, 0x02};

} // namespace

TEST_CASE("Loading a binary that needs another core creates, resets and loads exactly once")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    host.emuChanged = host.machineResets = host.romLoaded = 0;
    const auto delta = countDuring(host, [&] { REQUIRE(host.loadBinary("test.c8x", CODE, emu::EmuHostEx::None)); });
    CHECK(host.getProperties().propertyClass() == "COSMAC-VIP");
    CHECK(delta.coreCreations == 1);
    CHECK(delta.resets == 1);
    CHECK(delta.programLoads == 1);
    CHECK(host.emuChanged == 1);
    CHECK(host.machineResets == 1);
    CHECK(host.romLoaded == 1);
}

TEST_CASE("Loading without changing options keeps the core and resets once")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    auto* memory = host.emuCore().memory();
    host.emuChanged = 0;
    const auto delta = countDuring(host, [&] { REQUIRE(host.loadBinary("test.ch8", CODE, emu::EmuHostEx::DontChangeOptions)); });
    CHECK(delta.coreCreations == 0);
    CHECK(delta.resets == 1);
    CHECK(delta.programLoads == 1);
    CHECK(host.emuChanged == 0);
    CHECK(host.emuCore().memory() == memory);
    CHECK(memory[0x200] == CODE[0]);
}

TEST_CASE("Requesting the active configuration again does nothing")
{
    LifecycleHost host;
    const auto props = emu::CoreRegistry::propertiesForPreset("chip-8");
    host.updateEmulatorOptions(props);
    REQUIRE(host.loadBinary("test.ch8", CODE, emu::EmuHostEx::DontChangeOptions));
    const auto delta = countDuring(host, [&] { host.updateEmulatorOptions(props); });
    CHECK(delta.coreCreations == 0);
    CHECK(delta.resets == 0);
    CHECK(delta.programLoads == 0);
}

TEST_CASE("Reloading resets and loads the current program without recreating the core")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    REQUIRE(host.loadBinary("test.ch8", CODE, emu::EmuHostEx::DontChangeOptions));
    host.emuCore().memory()[0x200] = 0;
    host.romLoaded = host.machineResets = 0;
    const auto delta = countDuring(host, [&] { host.reloadProgram(); });
    CHECK(delta.coreCreations == 0);
    CHECK(delta.resets == 1);
    CHECK(delta.programLoads == 1);
    CHECK(host.machineResets == 1);
    CHECK(host.romLoaded == 0);
    CHECK(host.emuCore().memory()[0x200] == CODE[0]);
}

TEST_CASE("A program that does not fit leaves the machine untouched")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    host.emuCore().memory()[0x200] = 0xA5;
    const std::vector<uint8_t> tooBig(host.emuCore().memSize(), 0x11);
    const auto delta = countDuring(host, [&] { CHECK_FALSE(host.loadBinary("big.ch8", tooBig, emu::EmuHostEx::DontChangeOptions)); });
    CHECK(delta.resets == 0);
    CHECK(host.emuCore().memory()[0x200] == 0xA5);
}

TEST_CASE("A failed load does not leave its configuration request behind")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    const auto delta = countDuring(host, [&] {
        CHECK_FALSE(host.loadBinary("unknown.xyz", CODE, emu::EmuHostEx::None));
        host.reloadProgram();
    });
    CHECK(delta.coreCreations == 0);
    CHECK(host.getProperties().propertyClass() == "CHIP-8 GENERIC");
}

TEST_CASE("Without a configuration the host falls back to a default core")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::Properties{});
    CHECK(host.getProperties().propertyClass() == "CHIP-8 GENERIC");
    REQUIRE(host.loadBinary("test.ch8", CODE, emu::EmuHostEx::DontChangeOptions));
    CHECK(host.emuCore().memory()[0x200] == CODE[0]);
}

TEST_CASE("Loading never disassembles, the disassembly key tells when a new one is needed")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    REQUIRE(host.loadBinary("test.ch8", CODE, emu::EmuHostEx::DontChangeOptions));
    CHECK(host.lastSource.empty());
    const auto key = host.disassemblyKey();
    REQUIRE(key);
    CHECK(key->sha1 == calculateSha1(CODE));
    CHECK(key->address == 0x200);
    host.reloadProgram();
    CHECK(host.disassemblyKey() == key);
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("schipc"));
    REQUIRE(host.disassemblyKey());
    CHECK(host.disassemblyKey()->sha1 == key->sha1);
    CHECK_FALSE(host.disassemblyKey() == key);
}

TEST_CASE("A disassembly is created from a self contained request")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    REQUIRE(host.loadBinary("test.ch8", CODE, emu::EmuHostEx::DontChangeOptions));
    auto request = host.disassemblyRequest();
    REQUIRE(request);
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("schipc")); // the request must not depend on the host anymore
    const auto source = emu::EmuHostEx::disassemble(*request);
    CHECK(source.find(": main") != std::string::npos);
    CHECK(source.find("v0 := 0x2a") != std::string::npos);
}

TEST_CASE("Programs from the editor have no disassembly")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    REQUIRE(host.loadFromEditor(CODE));
    CHECK_FALSE(host.disassemblyKey());
    CHECK(host.emuCore().memory()[0x200] == CODE[0]);
}

TEST_CASE("The palette always matches the number of colors of the core")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("megachip"));
    CHECK(host.palette().size() == static_cast<size_t>(host.emuCore().getMaxColors()));
    REQUIRE(host.loadBinary("test.mc8", CODE, emu::EmuHostEx::DontChangeOptions));
    CHECK(host.palette().size() == static_cast<size_t>(host.emuCore().getMaxColors()));
    CHECK(host.palette() == host.propertiesPalette());
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    CHECK(host.palette().size() == 2);
    CHECK(host.palette() == host.propertiesPalette());
}

TEST_CASE("A VIP reset with C held boots the monitor without changing the configuration")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("vip-chip-8"));
    REQUIRE(host.loadBinary("test.ch8", CODE, emu::EmuHostEx::DontChangeOptions));
    const auto configured = host.getProperties();
    const auto* memory = host.emuCore().memory();
    auto interpreterLoaded = [&] { return std::any_of(memory, memory + 0x10, [](uint8_t b) { return b != 0; }); };
    REQUIRE(interpreterLoaded());
    // Holding C on the VIP keypad during reset boots the VIP monitor instead of the CHIP-8 interpreter, for this reset only.
    host.keyDown = 12;
    auto delta = countDuring(host, [&] { host.reloadProgram(); });
    host.keyDown.reset();
    CHECK(delta.coreCreations == 0);
    CHECK(host.getProperties() == configured);
    CHECK_FALSE(interpreterLoaded());
    CHECK(memory[0x200] == CODE[0]);
    // The next reset starts the configured interpreter again.
    host.reloadProgram();
    CHECK(interpreterLoaded());
    CHECK(host.getProperties() == configured);
}
