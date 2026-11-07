#include <doctest/doctest.h>
#include <emuhostex.hpp>
#include <configuration.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
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

TEST_CASE("Shared file extensions prefer the generic core")
{
    for (const auto& [extension, preset] : {std::pair{".ch8", "CHIP-8"}, std::pair{".c8x", "CHIP-8X"}}) {
        INFO(extension);
        CHECK(emu::CoreRegistry::propertiesForExtension(extension) == emu::CoreRegistry::propertiesForPreset(preset));
        CHECK(emu::CoreRegistry::presetForExtension(extension) == preset);
    }
}

TEST_CASE("Loading a binary that needs another core creates, resets and loads exactly once")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    host.emuChanged = host.machineResets = host.romLoaded = 0;
    const auto delta = countDuring(host, [&] { REQUIRE(host.loadBinary("test.c8xtpd", CODE, emu::EmuHostEx::None)); });
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

namespace {

// A CHIP-8 binary format (CBF) container with CHIP-8 and XO-CHIP bytecode, a palette and a speed
std::vector<uint8_t> makeC8B(const std::vector<uint8_t>& chip8, const std::vector<uint8_t>& xoChip, uint32_t speed)
{
    std::vector<uint8_t> data{'C', 'B', 'F', 0, 6, 17};
    auto word = [&](size_t value) { data.push_back(value >> 8); data.push_back(value & 0xFF); };
    const size_t paletteOffset = 24, speedOffset = 37, chip8Offset = 40, xoOffset = chip8Offset + chip8.size();
    data.push_back(0x01), word(chip8Offset), word(chip8.size());  // C8V_CHIP_8
    data.push_back(0x34), word(xoOffset), word(xoChip.size());    // C8V_XO_CHIP
    data.push_back(0);
    data.push_back(9), word(paletteOffset);  // palette
    data.push_back(1), word(speedOffset);    // execution speed
    data.push_back(0);
    REQUIRE(data.size() == paletteOffset);
    data.insert(data.end(), {4, 0x99, 0x66, 0x00, 0xFF, 0x66, 0x00, 0xFF, 0xCC, 0x00, 0x66, 0x22, 0x00});
    data.insert(data.end(), {uint8_t(speed >> 16), uint8_t(speed >> 8), uint8_t(speed)});
    REQUIRE(data.size() == chip8Offset);
    data.insert(data.end(), chip8.begin(), chip8.end());
    data.insert(data.end(), xoChip.begin(), xoChip.end());
    return data;
}

}

TEST_CASE("Every CBF variant has a generic preset")
{
    for (const auto name : {"xo-chip", "megachip", "schip-1-1", "schipc", "schip-1-0", "chip-48", "chip-8-x", "chip-8-e", "chip-8-fpd", "chip-8-tpd", "chip-10", "chip-8"}) {
        INFO(name);
        CHECK_FALSE(emu::CoreRegistry::propertiesForPreset(name).propertyClass().empty());
    }
}

TEST_CASE("A .c8b file loads its preferred bytecode with its palette and speed")
{
    const std::vector<uint8_t> xoCode{0x60, 0x11, 0x12, 0x02};
    const auto c8b = makeC8B(CODE, xoCode, 100000);
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    REQUIRE(host.loadBinary("test.c8b", c8b, emu::EmuHostEx::None));
    const auto expected = emu::CoreRegistry::propertiesForPreset("xo-chip");
    CHECK(host.getProperties().propertyClass() == expected.propertyClass());
    CHECK(emu::CoreRegistry::variantIndex(host.getProperties()).index == emu::CoreRegistry::variantIndex(expected).index);
    CHECK(host.getProperties()[std::string(emu::propid::INSTRUCTIONS_PER_FRAME)].getInt() == 100000);  // needs all 24 bits
    CHECK(std::equal(xoCode.begin(), xoCode.end(), host.emuCore().memory() + 0x200));
    REQUIRE(host.propertiesPalette().size() >= 4);
    CHECK(host.propertiesPalette().colors[0] == emu::Palette::Color(0x99, 0x66, 0x00));
    CHECK(host.propertiesPalette().colors[3] == emu::Palette::Color(0x66, 0x22, 0x00));
    CHECK(host.palette().colors[1] == emu::Palette::Color(0xFF, 0x66, 0x00));
}

TEST_CASE("A .c8b file that isn't a CBF container is rejected")
{
    LifecycleHost host;
    host.updateEmulatorOptions(emu::CoreRegistry::propertiesForPreset("chip-8"));
    CHECK_FALSE(host.loadBinary("broken.c8b", CODE, emu::EmuHostEx::None));
}
