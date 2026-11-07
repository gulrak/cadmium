#include <doctest/doctest.h>
#include <emulation/hardware/hp48/hpsaturnl2.hpp>
#include <emulation/hp48sx.hpp>
#include <emulation/hardware/hp48/g48statefile.hpp>
#include <emulation/hardware/hp48/g48sxmachine.hpp>
#include <emulation/hardware/hp48/display.hpp>
#include <emulation/videoscreen.hpp>
#include <resourcemanager.hpp>
#include "chip8headless.hpp"

#include <raylib.h>
#include <stdendian/stdendian.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <vector>

namespace {

class Hp48TestHost : public HeadlessTestHost
{
public:
    using HeadlessTestHost::HeadlessTestHost;
    bool isHeadless() const override { return false; }
};

class SaturnDisassemblyBus final : public hp48::ISystemBus
{
public:
    SaturnDisassemblyBus() : _memory(0x100000, 0xf) {}
    uint8_t readNibble(uint32_t address) override { return _memory[address & 0xfffffU]; }
    void writeNibble(uint32_t address, uint8_t value) override { _memory[address & 0xfffffU] = value & 0xfU; }
    void executeResetOpcode() override {}
    void executeConfigOpcode(uint32_t) override {}
    void executeUnconfigOpcode(uint32_t) override {}
    void executeCIdOpcode(uint64_t&) override {}

    void setCode(uint32_t address, std::string_view nibbles)
    {
        for (char nibble : nibbles)
            writeNibble(address++, static_cast<uint8_t>(nibble <= '9' ? nibble - '0' : nibble - 'A' + 10));
    }

private:
    std::vector<uint8_t> _memory;
};

struct Hp48Fixture
{
    explicit Hp48Fixture(const char* preset, const std::function<void(emu::Properties&)>& setup = {})
        : properties(prepared(preset, setup))
        , host(properties)
        , core(host, properties)
    {
    }

    static emu::Properties prepared(const char* preset, const std::function<void(emu::Properties&)>& setup)
    {
        auto properties = emu::CoreRegistry::propertiesForPreset(preset);
        if (setup)
            setup(properties);
        return properties;
    }

    std::vector<uint32_t> screen()
    {
        std::vector<uint32_t> pixels(core.getCurrentScreenWidth() * core.getCurrentScreenHeight());
        core.getRGBASpan({pixels.data(), core.getCurrentScreenWidth(), core.getCurrentScreenHeight()});
        return pixels;
    }

    void runInstructions(int count)
    {
        core.setExecMode(emu::GenericCpu::eRUNNING);
        core.executeInstructions(count);
        INFO(core.errorMessage());
        REQUIRE_FALSE(core.inErrorState());
    }

    emu::Properties properties;
    Hp48TestHost host;
    emu::Hp48sxEmulationCore core;
};

}  // namespace

TEST_CASE("Saturn debugger disassembly formats operands without executing the CPU")
{
    SaturnDisassemblyBus bus;
    hp48::SaturnL2 cpu(bus);
    cpu.reset();
    const auto initialCycles = cpu.cycles();
    const auto initialPc = cpu.getPC();

    const auto checkInstruction = [&](uint32_t address, std::string_view code, std::string_view expected) {
        bus.setCode(address, code);
        int nibbles = 0;
        const auto line = cpu.disassembleInstructionWithBytes(static_cast<int32_t>(address), &nibbles);
        INFO(line);
        CHECK(nibbles == code.size());
        CHECK(line.substr(0, 5) == fmt::format("{:05X}", address));
        CHECK(line.substr(7, code.size()) == code);
        CHECK(line.substr(cpu.disassemblyPrefixSize()) == expected);
        CHECK(line.find('{') == std::string::npos);
        CHECK(cpu.cycles() == initialCycles);
        CHECK(cpu.getPC() == initialPc);
    };

    checkInstruction(0x81234, "420", "nop3");
    checkInstruction(0x81234, "A70", "add.w b,a");
    checkInstruction(0x81234, "3401234", "move.p5 #43210,c");
    checkInstruction(0x81234, "8D54321", "jump.a #12345");
    checkInstruction(0x81234, "6100", "jump.3 #81236");
    checkInstruction(0x81234, "6FFF", "jump.3 #81234");
    checkInstruction(0xffffe, "6100", "jump.3 #00000");

    // A negative address means the current PC, as with Cadmium's other cores.
    bus.setCode(0, "420");
    CHECK(cpu.disassembleInstructionWithBytes(-1, nullptr).substr(cpu.disassemblyPrefixSize()) == "nop3");
    bus.setCode(0x81234, "80811");  // No instruction in the decoder's table.
    int nibbles = 0;
    CHECK(cpu.disassembleInstructionWithBytes(0x81234, &nibbles).substr(cpu.disassemblyPrefixSize()) == "???");
    CHECK(nibbles == 1);
    CHECK(cpu.cycles() == initialCycles);
}

TEST_CASE("HP48 backend exposes resolved Saturn disassembly at the interpreter entry")
{
    Hp48Fixture fixture("hp-48-schip-1-1");
    constexpr std::array<uint8_t, 4> PROGRAM{0x60, 0x07, 0x12, 0x00};
    REQUIRE(fixture.core.loadData(PROGRAM, {}));
    auto& cpu = fixture.core.getBackendCpu();
    const auto initialCycles = fixture.core.machineCycles();
    const auto initialPc = cpu.getPC();
    REQUIRE(initialPc > 0xffff);
    auto address = initialPc;
    for (int i = 0; i < 50; ++i) {
        int nibbles = 0;
        const auto line = cpu.disassembleInstructionWithBytes(address, &nibbles);
        INFO(line);
        CHECK(line.starts_with(fmt::format("{:05X}", address)));
        CHECK(line.find('{') == std::string::npos);
        REQUIRE(nibbles > 0);
        CHECK(line.size() > cpu.disassemblyPrefixSize());
        address += nibbles;
    }
    CHECK(cpu.getPC() == initialPc);
    CHECK(fixture.core.machineCycles() == initialCycles);
}

TEST_CASE("HP48 unloaded interpreter shows a static calculator stack")
{
    for (const auto preset : {"hp-48-chip-48", "hp-48-schip-1-0-beta", "hp-48-schip-1-0", "hp-48-schip-1-1", "hp-48-schpc"}) {
        INFO(std::string(preset));
        Hp48Fixture fixture(preset);
        auto& core = fixture.core;
        core.IEmulationCore::reset();
        REQUIRE_FALSE(core.inErrorState());
        const auto idleScreen = fixture.screen();
        CHECK(idleScreen.front() == be32(0xfbf4a5ff));
        CHECK(std::ranges::count(idleScreen, be32(0x0025b7ff)) > 100);
        const auto initialCycles = core.machineCycles();
        const auto initialPc = core.getBackendCpu().getPC();
        for (const auto backend : {false, true}) {
            core.setFocussedExecutionUnit(backend ? &core.getBackendCpu() : &core);
            for (const auto mode : {emu::GenericCpu::eRUNNING, emu::GenericCpu::eSTEP, emu::GenericCpu::eSTEPOVER, emu::GenericCpu::eSTEPOUT}) {
                core.setExecMode(mode);
                CHECK(core.execMode() == emu::GenericCpu::ePAUSED);
                CHECK(core.executeInstruction() == 0);
                core.executeInstructions(10);
                core.executeFrame();
                CHECK(core.machineCycles() == initialCycles);
                CHECK(core.getBackendCpu().getPC() == initialPc);
                CHECK(core.cycles() == 0);
                CHECK(core.frames() == 0);
                CHECK(fixture.screen() == idleScreen);
            }
        }
        core.IEmulationCore::reset(false);
        REQUIRE(core.loadData({}, {}));  // Empty toolbar reload stays idle.
        CHECK(fixture.screen() == idleScreen);
        if (std::string_view(preset) == "hp-48-schip-1-1") {
            if (const auto filename = std::getenv("CADMIUM_HP48_IDLE_SCREENSHOT")) {
                auto pixels = idleScreen;
                Image image{pixels.data(), core.getCurrentScreenWidth(), core.getCurrentScreenHeight(), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                REQUIRE(ExportImage(image, filename));
            }
        }
    }
}

TEST_CASE("HP48 interpreter reset restores the loaded program and machine state")
{
    for (const auto preset : {"hp-48-chip-48", "hp-48-schip-1-0-beta", "hp-48-schip-1-0", "hp-48-schip-1-1", "hp-48-schpc"}) {
        SUBCASE(preset)
        {
            INFO(std::string(preset));
            Hp48Fixture fixture(preset);
            auto& core = fixture.core;
            core.IEmulationCore::reset();
            INFO(core.errorMessage());
            REQUIRE_FALSE(core.inErrorState());
            CHECK(core.getPC() == 0x200);

            // Change registers, timer, RAM and LCD before returning to the loop.
            constexpr std::array<uint8_t, 18> PROGRAM{0x60, 0x07, 0x61, 0x03, 0xa3, 0x00, 0xf0, 0x55, 0xd0, 0x11, 0xf0, 0x15, 0x70, 0x01, 0x12, 0x0c, 0x12, 0x10};
            REQUIRE(core.loadData(PROGRAM, {}));
            const auto initialState = core.dumpStateLine();
            const auto initialScreen = fixture.screen();
            const std::vector<uint8_t> initialMemory(core.memory(), core.memory() + core.memSize());
            const auto initialMachineCycles = core.machineCycles();
            const auto initialBackend = core.getBackendCpu().getPC();
            const auto initialSaturn = static_cast<hp48::SaturnL2&>(core.getBackendCpu()).getState();
            core.setBreakpoint(0x210, {.label = "Retained breakpoint"});

            fixture.runInstructions(8);
            CHECK(core.getV(0) != 0);
            CHECK(core.memory()[0x300] == 7);

            for (const auto cold : {false, true}) {
                INFO(cold);
                fixture.host.keyDown(1);
                fixture.runInstructions(1);
                core.memory()[0x250] = 0xff;  // Include an unflushed debugger edit.
                core.IEmulationCore::reset(cold);
                INFO(core.errorMessage());
                REQUIRE_FALSE(core.inErrorState());
                CHECK(core.execMode() == emu::GenericCpu::ePAUSED);
                CHECK(core.cycles() == 0);
                CHECK(core.frames() == 0);
                CHECK(core.dumpStateLine() == initialState);
                CHECK(core.machineCycles() == initialMachineCycles);
                CHECK(core.getBackendCpu().getPC() == initialBackend);
                const auto saturn = static_cast<hp48::SaturnL2&>(core.getBackendCpu()).getState();
                CHECK(saturn.rstk == initialSaturn.rstk);
                CHECK(saturn.crc == initialSaturn.crc);
                CHECK(fixture.screen() == initialScreen);
                CHECK(std::equal(initialMemory.begin(), initialMemory.end(), core.memory()));
                CHECK(core.findBreakpoint(0x210) != nullptr);
                fixture.host.keyUp(1);
                fixture.runInstructions(8);
                CHECK(core.memory()[0x300] == 7);
            }

            // Left-click restart resets the core and then reloads the same image.
            core.IEmulationCore::reset();
            REQUIRE(core.loadData(PROGRAM, {}));
            CHECK(core.dumpStateLine() == initialState);
            CHECK(fixture.screen() == initialScreen);
            constexpr std::array<uint8_t, 4> REPLACEMENT{0x60, 0x2a, 0x12, 0x02};
            REQUIRE(core.loadData(REPLACEMENT, {}));
            fixture.runInstructions(1);
            CHECK(core.getV(0) == 42);
            core.IEmulationCore::reset();
            CHECK(core.memory()[0x201] == 42);
            CHECK(core.getPC() == 0x200);
            REQUIRE(core.loadData({}, {}));
            CHECK(core.getPC() == 0x200);
            CHECK(core.memory()[0x201] == 0);
            core.setExecMode(emu::GenericCpu::eRUNNING);
            CHECK(core.execMode() == emu::GenericCpu::ePAUSED);
            CHECK(core.executeInstruction() == 0);
        }
    }
}

TEST_CASE("HP48 SCHIP 1.0 beta selects the large font through Fx29")
{
    const auto fontAddress = [](const char* preset, uint8_t value, uint8_t opcodeLow) {
        Hp48Fixture fixture(preset);
        const std::array<uint8_t, 6> program{0x60, value, 0xf0, opcodeLow, 0x12, 0x04};
        REQUIRE(fixture.core.loadData(program, {}));
        fixture.runInstructions(2);
        return fixture.core.getI();
    };

    CHECK(fontAddress("hp-48-schip-1-0-beta", 0x05, 0x29) == 0x19);
    CHECK(fontAddress("hp-48-schip-1-0-beta", 0x15, 0x29) == 0x82);
    CHECK(fontAddress("hp-48-schip-1-0", 0x15, 0x29) == 0x19);
    CHECK(fontAddress("hp-48-schip-1-0", 0x05, 0x30) == 0x82);
}

TEST_CASE("HP48 interpreters see every host key under its own CHIP-8 key number")
{
    // Scans all keys in a loop, on a pressed one it sets I to its font digit.
    const std::array<uint8_t, 18> program{
        0x60, 0x00,  // 200: v0 := 0
        0xe0, 0x9e,  // 202: skip if key v0 is pressed
        0x12, 0x08,  // 204: jump 208
        0xf0, 0x29,  // 206: i := hex v0
        0x70, 0x01,  // 208: v0 += 1
        0x40, 0x10,  // 20A: skip if v0 != 16
        0x60, 0x00,  // 20C: v0 := 0
        0x12, 0x02,  // 20E: jump 202
        0x00, 0x00,
    };
    for (const auto* preset : {"hp-48-chip-48", "hp-48-schip-1-0-beta", "hp-48-schip-1-0", "hp-48-schip-1-1", "hp-48-schpc"}) {
        CAPTURE(preset);
        Hp48Fixture fixture(preset);
        auto& core = fixture.core;
        REQUIRE(core.loadData(program, {}));
        const auto runFrames = [&](int frames) {
            for (int frame = 0; frame < frames; ++frame) {
                core.setExecMode(emu::GenericCpu::eRUNNING);
                core.executeFrame();
                REQUIRE_FALSE(core.inErrorState());
            }
        };
        for (uint8_t key = 0; key < 16; ++key) {
            CAPTURE(static_cast<int>(key));
            fixture.host.keyDown(key);
            runFrames(12);
            CHECK(core.getI() == key * 5);
            fixture.host.keyUp(key);
            runFrames(4);
        }
    }
}

TEST_CASE("HP48 raw preset reset restores a ready calculator and loaded object")
{
    // The replacement ROM only runs the CHIP-8 interpreters, without HP's ROM the preset is hidden.
    if (!ResourceManager::instance().resourceAvailable("sxrom-rev-e")) {
        CHECK(emu::CoreRegistry::propertiesForPreset("hp-48-none").propertyClass().empty());
        return;
    }
    Hp48Fixture fixture("hp-48-none");
    auto& core = fixture.core;
    core.IEmulationCore::reset();
    INFO(core.errorMessage());
    REQUIRE_FALSE(core.inErrorState());
    const auto initialScreen = fixture.screen();
    const auto initialPc = core.getBackendCpu().getPC();
    const auto initialCycles = core.machineCycles();
    CHECK(initialCycles > 0);
    fixture.runInstructions(100);
    core.IEmulationCore::reset(false);
    CHECK(core.getBackendCpu().getPC() == initialPc);
    CHECK(core.machineCycles() == initialCycles);
    CHECK(fixture.screen() == initialScreen);

    // HPHP48-E transfer containing the real number zero (prolog 02933).
    constexpr std::array<uint8_t, 19> OBJECT{'H', 'P', 'H', 'P', '4', '8', '-', 'E', 0x33, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    REQUIRE(core.loadData(OBJECT, {}));
    const auto objectScreen = fixture.screen();
    REQUIRE(objectScreen != initialScreen);
    fixture.runInstructions(100);
    core.IEmulationCore::reset();
    CHECK(fixture.screen() == objectScreen);
}

TEST_CASE("HP48 SCHIP reset restarts a supplied ROM")
{
    const auto romPath = std::getenv("CADMIUM_HP48_TEST_ROM");
    if (!romPath)
        return;
    std::ifstream stream(romPath, std::ios::binary);
    REQUIRE(stream);
    const std::vector<uint8_t> program{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    Hp48Fixture fixture("hp-48-schip-1-1");
    auto& core = fixture.core;
    REQUIRE(core.loadData(program, {}));
    const auto initialState = core.dumpStateLine();
    const auto initialScreen = fixture.screen();
    for (int frame = 0; frame < 120; ++frame) {
        core.setExecMode(emu::GenericCpu::eRUNNING);
        core.executeFrame();
    }
    INFO(core.errorMessage());
    REQUIRE_FALSE(core.inErrorState());
    CHECK(fixture.screen() != initialScreen);
    core.IEmulationCore::reset();
    CHECK(core.getPC() == 0x200);
    CHECK(core.dumpStateLine() == initialState);
    CHECK(fixture.screen() == initialScreen);
    REQUIRE(core.loadData(program, {}));
    fixture.runInstructions(1);
}

TEST_CASE("HP48 fixed contrast replaces the calculator contrast live")
{
    // hp-48-none renders the emulated LCD, hp-48-schip-1-1 the static stack of the unloaded interpreter.
    // Without HP's ROM there is no hp-48-none preset.
    std::vector<const char*> presets{"hp-48-schip-1-1"};
    if (ResourceManager::instance().resourceAvailable("sxrom-rev-e")) {
        presets.insert(presets.begin(), "hp-48-none");
    }
    for (const auto preset : presets) {
        INFO(std::string(preset));
        Hp48Fixture fixture(preset);
        auto& core = fixture.core;
        core.IEmulationCore::reset();
        REQUIRE_FALSE(core.inErrorState());
        emu::CoreRegistry::applyRules(fixture.properties);
        CHECK_FALSE(fixture.properties.at("contrast").isActive());

        // At the default contrast of the ROM (11) the display has its nominal colors
        const auto nominal = fixture.screen();
        REQUIRE(std::ranges::count(nominal, be32(0x0025b7ff)) > 100);

        auto setContrast = [&](bool fixed, int contrast) {
            fixture.properties.at("fixedContrast").setBool(fixed);
            auto& prop = fixture.properties.at("contrast");
            prop.setInt(contrast);
            emu::CoreRegistry::applyRules(fixture.properties, prop.getId());
            CHECK(prop.isActive() == fixed);
            CHECK_FALSE(core.updateProperties(fixture.properties, prop));
        };
        setContrast(true, 31);
        const auto dark = fixture.screen();
        CHECK(std::ranges::count(dark, be32(0x0025b7ff)) == 0);
        CHECK(std::ranges::count(dark, be32(0x000a2aff)) > 100);
        setContrast(true, 2);
        const auto faded = fixture.screen();
        CHECK(std::ranges::all_of(faded, [](uint32_t color) { return color == be32(0xfbf4a5ff); }));
        setContrast(false, 2);
        CHECK(fixture.screen() == nominal);
    }
}

TEST_CASE("HP48 state files with the removed HDW stall fields still load")
{
    auto& resources = ResourceManager::instance();
    const auto rom = resources.resourceForName(resources.resourceAvailable("sxrom-rev-e") ? "sxrom-rev-e" : "cadmium-hp48sx-rom");
    hp48::G48sxMachine machine(hp48::G48SX_RECOVERY_RESET_KEY_HOLD_INSTRUCTIONS, {rom.data(), rom.data() + rom.size()});
    const auto stateBytes = machine.saveStateBytes(false);
    auto state = hp48::g48statefile::stateJsonFromBytes(stateBytes);
    auto& hdw = state.at("hardware").at("hdw");
    CHECK_FALSE(hdw.contains("waitAsserted"));
    CHECK_FALSE(hdw.contains("waitPenaltyCycles"));

    // State files written before the cleanup have these, they are ignored now
    hdw["waitAsserted"] = true;
    hdw["waitPenaltyCycles"] = 16;
    REQUIRE_NOTHROW(machine.loadState(hp48::g48statefile::encodeStateJson(state, false)));
    CHECK(machine.saveStateBytes(false) == stateBytes);
}

namespace {

bool originalRomAvailable()
{
    return ResourceManager::instance().resourceAvailable("sxrom-rev-e");
}

void useReplacementRom(emu::Properties& properties)
{
    if (originalRomAvailable())
        properties.at("rom").setSelectedText("cadmium-hp48sx-rom");
}

std::vector<uint32_t> stackScreen(const hp48::ObjectStack& stack, uint16_t width, uint16_t height)
{
    std::array<uint8_t, 64 * 17> frameBuffer{};
    const std::vector<std::string> menu{"EDIT", "VIEW", "RCL", "STO", "PURGE", "CLEAR"};
    hp48::Hp48Display display{{frameBuffer.data(), frameBuffer.size()}, stack, menu};
    std::vector<uint32_t> pixels(width * height);
    display.render({pixels.data(), width, height}, hp48::Hp48Display::Mode::Stack);
    std::ranges::transform(pixels, pixels.begin(), emu::argb8888ToRgba8888);
    return pixels;
}

}

TEST_CASE("HP48 ROM property offers the replacement ROM")
{
    Hp48Fixture fixture("hp-48-schip-1-1");
    const auto& rom = fixture.properties.at("rom");
    if (originalRomAvailable()) {
        REQUIRE(std::holds_alternative<emu::Property::Combo>(rom.getValue()));
        CHECK(rom.hasFlags(emu::PropertyFlags::eWritable));
        CHECK(std::get<emu::Property::Combo>(rom.getValue()).options == std::vector<std::string>{"sxrom-rev-e", "cadmium-hp48sx-rom"});
        CHECK(rom.getSelectedText() == "sxrom-rev-e");
    }
    else {
        REQUIRE(std::holds_alternative<std::string>(rom.getValue()));
        CHECK_FALSE(rom.hasFlags(emu::PropertyFlags::eWritable));
        CHECK(rom.getString() == "cadmium-hp48sx-rom");
    }
}

TEST_CASE("HP48 replacement ROM shows the calculator stack after the program ends")
{
    const auto runToIdle = [](Hp48Fixture& fixture, std::span<const uint8_t> program) {
        auto& core = fixture.core;
        REQUIRE(core.loadData(program, {}));
        for (int frame = 0; frame < 30; ++frame) {
            core.setExecMode(emu::GenericCpu::eRUNNING);
            core.executeFrame();
            REQUIRE_FALSE(core.inErrorState());
        }
        // The replacement ROM idles in #20000-#2FFFF (spec section 4), the core shows its stack display there
        const auto pc = core.getBackendCpu().getPC();
        CHECK(pc >= 0x20000);
        CHECK(pc < 0x30000);
        return fixture.screen();
    };
    for (const auto* preset : {"hp-48-chip-48", "hp-48-schip-1-0-beta", "hp-48-schip-1-0", "hp-48-schip-1-1", "hp-48-schpc"}) {
        CAPTURE(preset);
        // 00FD ends the program and drops the program string, the stack of the entry frame is empty
        Hp48Fixture exitFixture(preset, useReplacementRom);
        const std::array<uint8_t, 2> exitProgram{0x00, 0xFD};
        const auto exitScreen = runToIdle(exitFixture, exitProgram);
        if (std::string_view(preset) == "hp-48-chip-48") {
            // CHIP-48 treats 00FD as an unknown opcode: its error exit drops the program string and
            // pushes the CHIP-8 PC as a system binary
            hp48::ObjectStack stack;
            stack.push(hp48::Symbol{"<202h>"});
            CHECK(exitScreen == stackScreen(stack, exitFixture.core.getCurrentScreenWidth(), exitFixture.core.getCurrentScreenHeight()));
        }
        else {
            CHECK(exitScreen == stackScreen({}, exitFixture.core.getCurrentScreenWidth(), exitFixture.core.getCurrentScreenHeight()));
            Hp48Fixture errorFixture(preset, useReplacementRom);
            const std::array<uint8_t, 2> errorProgram{0x00, 0xC0};
            const auto errorScreen = runToIdle(errorFixture, errorProgram);
            // 00C0 is an error exit in SCHIP: the program string is dropped, the CHIP-8 PC pushed
            hp48::ObjectStack stack;
            stack.push(hp48::Symbol{"<202h>"});
            CHECK(errorScreen == stackScreen(stack, errorFixture.core.getCurrentScreenWidth(), errorFixture.core.getCurrentScreenHeight()));
        }
    }
}
