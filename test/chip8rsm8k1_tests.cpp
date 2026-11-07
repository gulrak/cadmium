#include <doctest/doctest.h>
#include <emulation/chip8rsm8k1.hpp>
#include <emulation/coreregistry.hpp>

namespace {
struct RsmTestHost final : emu::EmulatorHost {
    std::array<bool,16> keys{};
    bool isHeadless() const override { return true; }
    int getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return keys.at(key); }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> empty{}; return empty; }
    void updateScreen() override {}
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>&) override {}
    void updatePalette(const std::vector<uint32_t>&,size_t) override {}
};
}
TEST_CASE("RS-M8K1 registry and bounded guest memory")
{
    RsmTestHost host;
    auto props = emu::CoreRegistry::propertiesForPreset("RS-MEGACHIP");
    REQUIRE(props);
    CHECK(props.propertyClass() == "RS-M8K1");
    auto [name, core] = emu::CoreRegistry::create(host, props, nullptr);
    REQUIRE(core);
    auto* rsm = dynamic_cast<emu::Chip8RSM8k1*>(core.get());
    REQUIRE(rsm);
    CHECK(rsm->frameRate() == 50);
    CHECK(rsm->getPC() == 0x200);
    const uint8_t code[] = {0x60,0x03,0xb2,0x34};
    REQUIRE(rsm->loadData(code,{}));
    rsm->executeInstructions(2);
    CHECK(rsm->getPC() == 0x437); // Original missing base subtraction.
    rsm->reset();
    rsm->setRegister(19,0x100);
    rsm->executeInstruction();
    CHECK(rsm->inErrorState());
    CHECK(rsm->execMode() == emu::GenericCpu::ePAUSED);
    CHECK_FALSE(rsm->errorMessage().empty());
}
TEST_CASE("RS-M8K1 key waits yield and use isKeyDown")
{
    RsmTestHost host;
    auto props = emu::Chip8RSM8k1Options{}.asProperties();
    auto core = std::make_unique<emu::Chip8RSM8k1>(host,props);
    const uint8_t wait[] = {0xf1,0x0a};
    REQUIRE(core->loadData(wait,{}));
    core->executeInstruction();
    CHECK(core->cpuState() == emu::GenericCpu::eWAIT);
    host.keys[0] = true;
    core->executeInstruction();
    CHECK(core->getPC() == 0x200);
    host.keys[0] = false;
    core->executeInstruction();
    CHECK(core->getV(1) == 0);
    CHECK(core->getPC() == 0x202);
    CHECK(core->cycles() == 1);
}

TEST_CASE("RS-M8K1 packed sprite spill reaches Cadmium screen")
{
    RsmTestHost host;
    auto props = emu::Chip8RSM8k1Options{}.asProperties();
    auto core = std::make_unique<emu::Chip8RSM8k1>(host,props);
    const uint8_t code[] = {0xa3,0,0x60,63,0x61,0,0xd0,0x11};
    REQUIRE(core->loadData(code,{}));
    core->memory()[0x300] = 255;
    core->executeInstructions(4);
    CHECK(core->getScreen()->getPixelIndex(63,0) == 1);
    CHECK(core->getScreen()->getPixelIndex(0,1) == 1);
    CHECK(core->getScreen()->getPixelIndex(6,1) == 1);
    CHECK(core->getScreen()->getPixelIndex(7,1) == 0);
    core->setRegister(19,0x206);
    core->executeInstruction();
    CHECK(core->getV(15) == 1);
    CHECK(core->getScreen()->getPixelIndex(0,1) == 0);
}
