#include <doctest/doctest.h>
#include <emulation/eti660.hpp>
#include <emulation/hardware/cdp186x.hpp>
#include "chip8headless.hpp"

#include <fstream>
#include <iterator>
#include <cstdlib>
#include <raylib.h>
#include <stdendian/stdendian.h>

namespace {
class VideoTestBus : public emu::Cdp1802Bus
{
public:
    uint8_t readByte(uint16_t) const override { return 0x80; }
    uint8_t readByteDMA(uint16_t address) const override
    {
        if ((address & 0xF000) == 0xD000) {
            ++colorReads;
            return 1; // Red attributes must be ignored before colour is enabled.
        }
        return 0x80;
    }
    void writeByte(uint16_t, uint8_t) override {}
    mutable int colorReads{};
};

class EtiTestHost : public HeadlessTestHost
{
public:
    using HeadlessTestHost::HeadlessTestHost;
    bool isKeyDown(uint8_t key) override
    {
        return key == KEY_ETI_STEP ? stepDown : HeadlessTestHost::isKeyDown(key);
    }
    void updateScreen() override { ++screenUpdates; }
    void vblank() override { ++vblanks; }
    bool stepDown{};
    int screenUpdates{};
    int vblanks{};
};

struct EtiFixture
{
    explicit EtiFixture(const char* preset = "eti-chip-8", bool cleanRam = true)
        : props(emu::CoreRegistry::propertiesForPreset(preset)), host(props)
    {
        props["Clean RAM"].setBool(cleanRam);
        auto [name, instance] = emu::CoreRegistry::create(host, props, nullptr);
        core = std::move(instance);
        REQUIRE(core);
        eti = dynamic_cast<emu::Eti660*>(core.get());
        REQUIRE(eti);
        core->reset();
        INFO(static_cast<emu::Cdp1802&>(eti->getBackendCpu()).dumpStateLine());
        INFO(eti->errorMessage());
        REQUIRE_FALSE(eti->inErrorState());
    }
    void runFrames(int count)
    {
        for (int frame = 0; frame < count; ++frame)
            core->executeFrame();
    }
    void captureScreen(const std::string& filename) const
    {
        if (const auto directory = std::getenv("CADMIUM_ETI_TEST_SCREENSHOT_DIR")) {
            const auto* screen = core->getScreenRGBA();
            std::vector<uint32_t> rgba(64 * 192);
            for (int y = 0; y < 192; ++y)
                for (int x = 0; x < 64; ++x)
                    rgba[y * 64 + x] = screen->getPixel(x, y);
            Image image{rgba.data(), 64, 192, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            const auto name = filename.substr(filename.find_last_of('/') + 1);
            REQUIRE(ExportImage(image, (std::string(directory) + '/' + name + ".png").c_str()));
        }
    }
    emu::Properties props;
    EtiTestHost host;
    std::unique_ptr<emu::IEmulationCore> core;
    emu::Eti660* eti{};
};
}

TEST_CASE("CDP1864 background colour is independent of colour-data fetching")
{
    for (const auto type : {emu::Cdp186x::eCDP1864, emu::Cdp186x::eVP590, emu::Cdp186x::eCDP1861}) {
        INFO(type);
        VideoTestBus bus;
        emu::Cdp1802 cpu(bus);
        emu::Cdp186x video(type, cpu, false);
        const auto render = [&] {
            video.enableDisplay();
            emu::Cdp1802State state{};
            state.ie = true;
            state.cycles = (78 * 14 + 2) * 8;
            cpu.setState(state);
            video.executeStep(); // Latch display enable at the interrupt window.
            state.ie = false;
            state.r[0] = 0x480;
            state.cycles = (80 * 14 + 4) * 8;
            cpu.setState(state);
            video.executeStep(); // DMA draws 0x80: one foreground bit per byte.
        };
        const auto checkPixels = [&](uint32_t foreground, uint32_t background) {
            CHECK(video.getScreen().getPixel(0, 0) == be32(foreground));
            CHECK(video.getScreen().getPixel(1, 0) == be32(background));
            CHECK(video.getScreen().getBackgroundRGBA() == be32(background));
        };
        render();
        checkPixels(0xFFFFFFFF, type == emu::Cdp186x::eCDP1861 ? 0x000000FF : 0x000080FF);
        CHECK(bus.colorReads == 0);
        if (type == emu::Cdp186x::eCDP1864) {
            video.incrementBackground();
            render();
            checkPixels(0xFFFFFFFF, 0x000000FF);
            CHECK(bus.colorReads == 0);
            video.enableColor();
            render();
            checkPixels(0xFF0000FF, 0x000000FF);
            CHECK(bus.colorReads == 8);
            video.incrementBackground();
            render();
            checkPixels(0xFF0000FF, 0x008000FF);
            CHECK(bus.colorReads == 16);
            video.reset();
            render();
            checkPixels(0xFFFFFFFF, 0x000080FF);
            CHECK(bus.colorReads == 16); // Reset disables attribute fetching again.
        }
    }
}

TEST_CASE("ETI660 debugger state is synchronized with interpreter RAM")
{
    EtiFixture fixture;
    fixture.eti->setRegister(0, 0x2A);
    fixture.eti->setRegister(16, 0x812);
    fixture.eti->setRegister(17, 10);
    fixture.eti->setRegister(18, 7);
    CHECK(fixture.eti->getV(0) == 0x2A);
    CHECK(fixture.eti->readByte(0x470) == 0x2A);
    CHECK(fixture.eti->getI() == 0x812);
    CHECK(fixture.eti->delayTimer() == 10);
    CHECK(fixture.eti->soundTimer() == 7);
    const uint8_t code[] = {0x70,0x01, 0x16,0x02};
    REQUIRE(fixture.core->loadData(code, {}));
    fixture.core->setExecMode(emu::GenericCpu::eSTEP);
    fixture.eti->executeInstruction();
    CHECK(fixture.eti->getV(0) == 0x2B);
    fixture.eti->setRegister(19, 0x1000);
    CHECK(fixture.eti->getPC() == 0x602);
    fixture.core->reset(false);
    CHECK(fixture.eti->getV(0) == 0x2B);
    CHECK(fixture.eti->getSP() == 0);
}

TEST_CASE("ETI660 boot and memory use hardware addresses")
{
    EtiFixture fixture;
    CHECK(fixture.core->name() == "ETI660");
    CHECK(fixture.core->numberOfExecutionUnits() == 2);
    CHECK(fixture.eti->getPC() == 0x600);
    CHECK(fixture.eti->getSP() == 0);
    CHECK(fixture.core->memSize() == 4096);
    CHECK(fixture.core->defaultLoadAddress() == 0x600);
    CHECK(fixture.core->frameRate() == 51);
    CHECK(fixture.eti->time().getClockFreq() == 1773448);
    CHECK(fixture.eti->supportedChip8Variants() == emu::chip8::Variant::CHIP_8_ETI660);
    CHECK(fixture.eti->readByte(0) == 0xF8);
    fixture.eti->writeByte(0, 0);
    CHECK(fixture.eti->readByte(0) == 0xF8);
    for (const auto address : {0x400, 0x600, 0xFFF}) {
        fixture.eti->writeByte(address, 0xA5);
        CHECK(fixture.core->memory()[address] == 0xA5);
        CHECK(fixture.eti->readByte(address) == 0xA5);
    }
    CHECK(fixture.eti->readMemoryByte(0x10000) == 0xFF);
    const uint8_t data[] = {0x60, 0x42};
    CHECK_FALSE(fixture.core->loadData(data, 0x3FF));
    CHECK_FALSE(fixture.core->loadData(data, 0xFFF));
    REQUIRE(fixture.core->loadData(data, {}));
    CHECK(fixture.eti->readByte(0x600) == 0x60);
    CHECK(fixture.eti->readByte(0x601) == 0x42);
    fixture.core->reset(false);
    CHECK(fixture.eti->readByte(0x601) == 0x42);
}

TEST_CASE("ETI660 monitor clears display RAM without Clean RAM")
{
    EtiFixture fixture("eti-chip-8", false);
    const uint8_t code[] = {0x16,0x00}; // The program itself never clears the screen.
    REQUIRE(fixture.core->loadData(code, {}));
    for (unsigned address = 0x480; address < 0x600; ++address)
        CHECK(fixture.eti->readByte(address) == 0);

    for (const bool cold : {false, true}) {
        INFO(cold);
        for (unsigned address = 0x480; address < 0x600; ++address)
            fixture.eti->writeByte(address, 0xFF);
        fixture.eti->writeByte(0x470, 0x2A);
        fixture.eti->writeByte(0x800, 0xA5);
        fixture.core->reset(cold);
        REQUIRE_FALSE(fixture.eti->inErrorState());
        CHECK(fixture.eti->getPC() == 0x600);
        CHECK(fixture.eti->getSP() == 0);
        CHECK(fixture.eti->readByte(0x470) == 0x2A);
        CHECK(fixture.eti->readByte(0x800) == 0xA5);
        CHECK(fixture.eti->readByte(0x600) == code[0]);
        CHECK(fixture.eti->readByte(0x601) == code[1]);
        for (unsigned address = 0x480; address < 0x600; ++address)
            CHECK(fixture.eti->readByte(address) == 0);
        fixture.runFrames(2);
        const auto* screen = fixture.core->getScreenRGBA();
        for (int y = 0; y < 192; ++y)
            for (int x = 0; x < 64; ++x)
                CHECK(screen->getPixel(x, y) == screen->getBackgroundRGBA());
    }
}

TEST_CASE("ETI660 single stepping follows the original interpreter and stack")
{
    EtiFixture fixture;
    const uint8_t code[] = {0x26,0x06, 0x60,0x0A, 0x16,0x04, 0x61,0x0B, 0x00,0xEE};
    REQUIRE(fixture.core->loadData(code, {}));
    fixture.core->setExecMode(emu::GenericCpu::eSTEP);
    fixture.eti->executeInstruction();
    CHECK(fixture.core->execMode() == emu::GenericCpu::ePAUSED);
    CHECK(fixture.eti->getPC() == 0x606);
    CHECK(fixture.eti->getSP() == 1);
    CHECK(fixture.eti->stackElement(0) == 0x602);
    fixture.core->setExecMode(emu::GenericCpu::eSTEP);
    fixture.eti->executeInstruction();
    CHECK(fixture.eti->getV(1) == 0x0B);
    fixture.core->setExecMode(emu::GenericCpu::eSTEP);
    fixture.eti->executeInstruction();
    CHECK(fixture.eti->getPC() == 0x602);
    CHECK(fixture.eti->getSP() == 0);
    fixture.core->setExecMode(emu::GenericCpu::eSTEP);
    fixture.eti->executeInstruction();
    CHECK(fixture.eti->getV(0) == 0x0A);
}

TEST_CASE("ETI660 raw monitor starts a loaded program with key 8")
{
    EtiFixture fixture("eti-none");
    CHECK(fixture.core->numberOfExecutionUnits() == 1);
    CHECK(fixture.core->focussedExecutionUnit() == &fixture.eti->getBackendCpu());
    CHECK(fixture.core->defaultLoadAddress() == 0x600);
    const uint8_t code[] = {0x60,0x0A, 0x61,0x0B, 0x16,0x04};
    REQUIRE(fixture.core->loadData(code, {}));
    fixture.runFrames(40);
    fixture.host.keyDown(8);
    fixture.runFrames(10);
    fixture.host.keyUp(8);
    fixture.runFrames(20);
    CHECK(fixture.eti->readByte(0x470) == 0x0A);
    CHECK(fixture.eti->readByte(0x471) == 0x0B);
    CHECK_FALSE(fixture.eti->inErrorState());
}

TEST_CASE("ETI660 sound and delay timers run from hardware interrupts")
{
    EtiFixture fixture;
    const uint8_t code[] = {0x60,0x0F, 0xF0,0x00, 0x61,0x06, 0xF1,0x18,
                           0xF1,0x15, 0xF1,0x07, 0x31,0x00, 0x16,0x0A, 0x16,0x10};
    REQUIRE(fixture.core->loadData(code, {}));
    fixture.runFrames(2);
    std::array<int16_t,256> samples{};
    fixture.core->renderAudio(samples.data(), samples.size(), 44100);
    CHECK(std::any_of(samples.begin(), samples.end(), [](int16_t value) { return value > 0; }));
    CHECK(std::any_of(samples.begin(), samples.end(), [](int16_t value) { return value < 0; }));
    fixture.runFrames(10);
    fixture.core->renderAudio(samples.data(), samples.size(), 44100);
    CHECK(std::all_of(samples.begin(), samples.end(), [](int16_t value) { return value == 0; }));
    CHECK(fixture.eti->delayTimer() == 0);
    CHECK(fixture.eti->soundTimer() == 0);
}

TEST_CASE("ETI660 original keypad scan reads every VIP-layout key")
{
    for (uint8_t key = 0; key < 16; ++key) {
        INFO(unsigned(key));
        EtiFixture fixture;
        const uint8_t code[] = {0xF0,0x0A, 0x16,0x02};
        REQUIRE(fixture.core->loadData(code, {}));
        fixture.runFrames(3);
        CHECK(fixture.eti->getPC() == 0x600);
        fixture.host.keyDown(key);
        fixture.runFrames(10);
        fixture.host.keyUp(key);
        fixture.runFrames(5);
        CHECK_FALSE(fixture.eti->inErrorState());
        CHECK(fixture.eti->getPC() == 0x602);
        CHECK(fixture.eti->getV(0) == key);
    }
}

#ifdef ETI660_EXAMPLES
TEST_CASE("ETI660 supplied CHIP-8 examples render through native DMA")
{
    for (const auto filename : {
             "Chip-8 ETI660 Programs/Pattern Maker [Feb 1982].ch8",
             "Chip-8 ETI660 Programs/Character Generator.ch8",
             "Chip-8 ETI660 Programs/Character Generator - Dump charset.ch8",
             "Chip-8 ETI660 Games/Space Invaders MK III [P. Collins, Nov 1982].ch8",
             "Chip-8 ETI660 Hybrids/Pakman (ETI660 Hybrid) [B. Kreykes, Sep 1983].ch8"}) {
        INFO(std::string(filename));
        std::ifstream file(std::string(ETI660_EXAMPLES) + '/' + filename, std::ios::binary);
        REQUIRE(file);
        const std::vector<uint8_t> code{std::istreambuf_iterator<char>(file), {}};
        EtiFixture fixture;
        REQUIRE(fixture.core->loadData(code, {}));
        int peakPixels = 0;
        for (int frame = 0; frame < 60; ++frame) {
            fixture.runFrames(1);
            int pixels = 0;
            const auto* screen = fixture.core->getScreenRGBA();
            for (int y = 0; y < 192; ++y)
                for (int x = 0; x < 64; ++x)
                    pixels += screen->getPixel(x, y) != screen->getBackgroundRGBA();
            const bool newPeak = pixels > peakPixels;
            peakPixels = std::max(peakPixels, pixels);
            if (newPeak)
                fixture.captureScreen(filename);
        }
        CHECK_FALSE(fixture.eti->inErrorState());
        CHECK(fixture.core->frames() >= 59);
        CHECK(fixture.host.screenUpdates >= 59);
        INFO(fixture.eti->dumpStateLine());
        CHECK(peakPixels > 0);
        const auto* screen = fixture.core->getScreenRGBA();
        // Pakman selects green for three lives without enabling colour RAM.
        const uint32_t background = std::string(filename).find("Pakman") != std::string::npos ? 0x008000FF : 0x000080FF;
        CHECK(screen->getBackgroundRGBA() == be32(background));
        for (int y = 0; y < 192; ++y)
            for (int x = 0; x < 64; ++x) {
                const auto pixel = be32(screen->getPixel(x, y));
                CHECK((pixel == 0xFFFFFFFF || pixel == background));
            }
    }
}

TEST_CASE("ETI660 Wipeout hybrid uses the colour board")
{
    std::ifstream file(std::string(ETI660_EXAMPLES) + "/Chip-8 ETI660 Hybrids/Wipeout (ETI660 hybrid) [W.F. Kreykes, 1982].ch8", std::ios::binary);
    REQUIRE(file);
    const std::vector<uint8_t> code{std::istreambuf_iterator<char>(file), {}};
    EtiFixture fixture;
    REQUIRE(fixture.core->loadData(code, {}));
    fixture.core->setExecMode(emu::GenericCpu::eSTEPOVER);
    fixture.runFrames(150);
    REQUIRE(fixture.core->execMode() == emu::GenericCpu::ePAUSED);
    // The colour setup writes PIA scratch address 0x439. It must return to
    // the instruction after 2822, rather than overwrite that return address.
    REQUIRE(fixture.eti->getPC() == 0x602);
    CHECK(fixture.eti->getSP() == 0);
    fixture.core->setExecMode(emu::GenericCpu::eRUNNING);
    fixture.runFrames(300);
    INFO(fixture.eti->dumpStateLine());
    INFO(static_cast<emu::Cdp1802&>(fixture.eti->getBackendCpu()).dumpStateLine());
    CHECK_FALSE(fixture.eti->inErrorState());
    std::set<uint32_t> colors;
    const auto* screen = fixture.core->getScreenRGBA();
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 64; ++x)
            colors.insert(screen->getPixel(x, y));
    CHECK(colors.size() >= 4);
    // The three six-row markers at x=10,30,50 match the Emma 02 header.
    // Check bitmap RAM and every repeated DMA line, excluding the live score.
    constexpr std::array<uint8_t,6> MARKER = {0x60, 0xF0, 0xF0, 0xF0, 0x60, 0x00};
    for (int y = 0; y < MARKER.size(); ++y) {
        for (int x = 0; x < 56; ++x) {
            INFO(x, y);
            bool expected = false;
            for (const int start : {10, 30, 50})
                if (x >= start && x < start + 8)
                    expected |= (MARKER[y] & (0x80 >> (x - start))) != 0;
            const auto bitmap = fixture.eti->readByte(0x480 + y * 8 + x / 8);
            CHECK(bool(bitmap & (0x80 >> (x & 7))) == expected);
            for (int repeat = 0; repeat < 4; ++repeat)
                CHECK((screen->getPixel(x, y * 4 + repeat) != screen->getBackgroundRGBA()) == expected);
        }
    }
    fixture.captureScreen("Wipeout");
}

#endif
