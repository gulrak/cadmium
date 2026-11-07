#include <doctest/doctest.h>
#include <emulation/studio2.hpp>
#include "chip8headless.hpp"

#include <cstdlib>
#include <raylib.h>

namespace {

struct Studio2Fixture
{
    Studio2Fixture()
        : props(emu::CoreRegistry::propertiesForPreset("studio2-builtin")), host(props)
    {
        props["cleanRam"].setBool(true);
        auto [name, instance] = emu::CoreRegistry::create(host, props, nullptr);
        core = std::move(instance);
        REQUIRE(core);
        studio2 = dynamic_cast<emu::Studio2*>(core.get());
        REQUIRE(studio2);
        core->reset();
        REQUIRE_FALSE(studio2->inErrorState());
    }
    void runFrames(int count)
    {
        for (int frame = 0; frame < count; ++frame)
            core->executeFrame();
    }
    int litPixels() const
    {
        const auto* screen = core->getScreenRGBA();
        const auto background = screen->getPixel(0, 0);
        int lit = 0;
        for (int y = 0; y < 128; ++y)
            for (int x = 0; x < 64; ++x)
                lit += screen->getPixel(x, y) != background;
        return lit;
    }
    void captureScreen(const std::string& name) const
    {
        if (const auto directory = std::getenv("CADMIUM_STUDIO2_TEST_SCREENSHOT_DIR")) {
            const auto* screen = core->getScreenRGBA();
            std::vector<uint32_t> rgba(64 * 128);
            for (int y = 0; y < 128; ++y)
                for (int x = 0; x < 64; ++x)
                    rgba[y * 64 + x] = screen->getPixel(x, y);
            Image image{rgba.data(), 64, 128, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            REQUIRE(ExportImage(image, (std::string(directory) + '/' + name + ".png").c_str()));
        }
    }
    emu::Properties props;
    HeadlessTestHost host;
    std::unique_ptr<emu::IEmulationCore> core;
    emu::Studio2* studio2{};
};

} // namespace

TEST_CASE("Studio II decodes ROM and mirrored RAM like the board")
{
    Studio2Fixture f;
    auto& bus = *f.studio2;

    CHECK(bus.readByte(0x0000) == emu::_studio2_rom[0]);
    CHECK(bus.readByte(0x07FF) == emu::_studio2_rom[0x7FF]);

    bus.writeByte(0x0812, 0x5A);
    CHECK(bus.readByte(0x0812) == 0x5A);
    CHECK(bus.readByte(0x0C12) == 0x5A);   // A10 is not decoded for RAM
    CHECK(bus.readByte(0x0A12) == 0xFF);   // A9 high: neither ROM nor RAM

    bus.writeByte(0x0934, 0xA5);
    CHECK(bus.readByte(0x0934) == 0xA5);   // A8 selects the second RAM page
    CHECK(bus.readByte(0x0834) == 0x00);

    bus.writeByte(0x0156, 0x77);           // ROM range with A9 low: the write reaches RAM
    CHECK(bus.readByte(0x0156) == emu::_studio2_rom[0x156]);
    CHECK(bus.readByte(0x0956) == 0x77);
}

TEST_CASE("Studio II boots the built-in ROM and enables the display")
{
    Studio2Fixture f;
    f.runFrames(120);
    INFO(static_cast<emu::Cdp1802&>(f.studio2->getBackendCpu()).dumpStateLine());
    CHECK_FALSE(f.studio2->inErrorState());
    CHECK(f.studio2->isDisplayEnabled());
    CHECK(f.studio2->frames() >= 119);
    CHECK(f.studio2->frameRate() == 60);
    f.captureScreen("studio2-boot");
}

TEST_CASE("Studio II starts the selected built-in game on a pad A key")
{
    Studio2Fixture f;
    auto dumpRam = [&](const char* label) {
        if (std::getenv("CADMIUM_STUDIO2_DUMP")) {
            std::printf("%s RAM:", label);
            for (int address = 0x800; address < 0xA00; ++address)
                std::printf("%s%02x", (address & 31) ? "" : "\n", f.studio2->readByte(address));
            std::printf("\n");
        }
    };
    f.runFrames(120);
    dumpRam("boot");
    CHECK(f.litPixels() == 0);
    f.host.keyDown(1);
    f.runFrames(10);
    f.host.keyUp(1);
    f.runFrames(110);
    dumpRam("after-key");
    CHECK_FALSE(f.studio2->inErrorState());
    // Doodle starts with its single drawing dot in the bottom row.
    CHECK(f.studio2->readByte(0x09F8) == 0x80);
    CHECK(f.litPixels() > 0);
    f.captureScreen("studio2-doodle");
}

TEST_CASE("Studio II built-in game 3 draws its playfield")
{
    Studio2Fixture f;
    f.runFrames(120);
    f.host.keyDown(3);
    f.runFrames(10);
    f.host.keyUp(3);
    f.runFrames(110);
    if (std::getenv("CADMIUM_STUDIO2_DUMP")) {
        std::printf("after-key RAM:");
        for (int address = 0x800; address < 0xA00; ++address)
            std::printf("%s%02x", (address & 31) ? "" : "\n", f.studio2->readByte(address));
        std::printf("\n");
    }
    CHECK_FALSE(f.studio2->inErrorState());
    CHECK(f.litPixels() > 100);
    f.captureScreen("studio2-game3");
}
